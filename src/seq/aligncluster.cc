#include "seq/stockholm.h"
#include "newmat/newmat.h"
#include "util/vector_output.h"
#include <string.h>

int main (int argc, char** argv)
{
  INIT_OPTS_LIST (opts, argc, argv, 2, "[options] (AMA|SPS|PPV|TCS) <stockholm database>", "Alignment database centroid tool");
 
  int nth;
  bool find_auto;
  opts.newline(); 
  opts.add ("n -only-nth", nth = 1, "only use every nth alignment in distance matrix");  
  opts.add ("auto", find_auto = true, "output time-averaged alignment similarity as a function of lag");  
  //bool flat_format;
  //bool scores_only;

  //opts.newline();
  //opts.add ("f -flat-format", flat_format = false, "output alignment statistics in a flat-format");
  //opts.add ("s -scores-only", scores_only = false, "output scores only, in AMA\\tSPS\\tPPV\\tTCS format");

  try
    {
      if (!opts.parse()) { CLOGERR << opts.short_help(); exit(1); }
    }
  catch (const Dart_exception& e)
    {
      CLOGERR << opts.short_help();
      CLOGERR << e.what();
      exit(1);
    }

  try
    {
    //  opts.parse_or_die();
      const string method = opts.args[0];
      if(!((method.compare("AMA")&&method.compare("SPS")&&method.compare("PPV")&&method.compare("TCS")) == 0)){
	CLOGERR << "ERROR: Invalid Comparison Measure: "<<method<<"\n"<< opts.short_help(); exit(1);
      } 
      CLOG(4) <<"Using comparison measure: "<<method<<"\n";
	
      CLOG(4) <<"Loading alignments\n";
      const sstring filename = opts.args[1];
      
      Stockholm_database stock_db, tmp_db; 
      Stockholm align1, align2;
      Sequence_database seq_db;

      ifstream file (filename.c_str());
      if (!file) THROWEXPR (filename << ": File not found");
      stock_db.read (file, seq_db);
      file.close();
      tmp_db = stock_db;
     
      CLOG(4) <<"Creating "<<(int) stock_db.size()/nth<<"^2 distance matrix\n";
      ArrayLengthSpecifier n(stock_db.size());
      SquareMatrix distance_matrix(n);
      int i = 1;
      int j = 1;
      for_const_contents(list<Stockholm>, stock_db.align, align1_tmp){
	if ((i % nth) > 0) {
		i++;
		tmp_db.align.pop_front();
		if(i > (int) stock_db.align.size()){
		  break;
		}
		continue;
	}
      	CLOG(3) <<"Computing row "<<i / nth<<"\n";
	distance_matrix(i / nth, i / nth ) = 1;
      	tmp_db.align.pop_front();
	j = i + 1;
      	for_const_contents(list<Stockholm>, tmp_db.align, align2_tmp){
	   if((j % nth) > 0) {
		j++;
		if(j > (int) stock_db.align.size()){
		  break;
		}
		continue;
	   }
	   Stockholm align1 (*align1_tmp);
	   Stockholm align2 (*align2_tmp);
      	   align1.erase_empty_columns();
      	   align2.erase_empty_columns();
	   CLOG(1)<< "Computing matrix entries: "<<i / nth<<","<<j / nth<<"\n";
	   if(method.compare("AMA") == 0){
		distance_matrix(i / nth, j / nth) = align1.ama_similarity(align2);
		distance_matrix(j / nth, i / nth) = distance_matrix(i,j);
	   }
	   else if(method.compare("SPS") == 0){
	   	const int res_pairs12 = align1.residue_pair_overlap (align2);
		const int res_pairs1 = align1.residue_pairs();
		const int res_pairs2 = align2.residue_pairs();
    	   	distance_matrix(i / nth, j / nth) = (float) res_pairs12 / (float) res_pairs1;
		distance_matrix(j / nth, i / nth) = (float) res_pairs12 / (float) res_pairs2;
	   }
	   else if(method.compare("PPV") == 0){
	   	const int res_pairs12 = align1.residue_pair_overlap (align2);
		const int res_pairs1 = align1.residue_pairs();
		const int res_pairs2 = align2.residue_pairs();
    	   	distance_matrix(i / nth, j / nth) = (float) res_pairs12 / (float) res_pairs2;
		distance_matrix(j / nth, i / nth) = (float) res_pairs12 / (float) res_pairs1;
	   }
	   else if(method.compare("TCS") == 0){
	   	const int cols1 = align1.columns();
           	const int cols2 = align2.columns();
           	const int col_overlap = align1.column_overlap(align2);
		distance_matrix(i / nth, j / nth) = col_overlap/cols1;
		distance_matrix(j / nth, i / nth) = col_overlap/cols2;
	   }
	   else { //sanity check, should never reach here
	   	THROWEXPR ("Invalid comparison method!");
	   }
	   j++;
	}
	i++;
     }
     
     // find centroid
     float max = 0.0;
     int max_index = -1;
     CLOG(4) << "Finding centroid...\n";
     for(int x = 1; x <= stock_db.size() / nth; x++){
     	const float this_sum = distance_matrix.Row(x).Sum();
	if (max_index < 0 || this_sum > max){
		max = this_sum;
		max_index = x;
	}
     }
     max = max / stock_db.size();

     // create alignment; annotate similarity & avg similarity
     Stockholm final (*stock_db.align_index[(max_index-1) * nth]);
     sstring tmp;
     final.add_gf_annot("Similarity Measure:", method);  // CODE SMELL: Stockholm tags can't include spaces!
     tmp << max;
     final.add_gf_annot("Average Similarity:", tmp);  // CODE SMELL: Stockholm tags can't include spaces!

     // annotate alignment with stationary autocorrelation-like measure, if requested
     if (find_auto) {
       vector<double> autovec;
       const int matrix_size = stock_db.size() / nth;
       for (int lag = 0; lag < matrix_size; lag++) {
	 double total = 0.;
	 for (int i = 1; i <= matrix_size - lag; i++)
	   total += distance_matrix (i, i + lag);
	 const double time_avg = total / (double) (matrix_size - lag);
	 autovec.push_back (time_avg);
       }
       sstring autovec_string;
       autovec_string << autovec;
       final.add_gf_annot("AutoSimilarity", autovec_string);  // CODE SMELL: Stockholm tag should be shorter & defined up top as a constant
     }

      // output final alignment
     final.write_Stockholm(cout); 
 
  }
  catch (const Dart_exception& e)
    {
      cerr << e.what();
      exit(1);
    }
  return 0;
}
