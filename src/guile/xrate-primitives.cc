#include <stdlib.h>
#include <libguile.h>

#include "seq/stockholm.h"
#include "tree/tree_alignment.h"
#include "util/sexpr.h"
#include "ecfg/ecfgmain.h"
#include "ecfg/ecfgsexpr.h"

#include "guile/stockholm-type.h"
#include "guile/xrate-primitives.h"

static void get_alphgram_sexpr (SCM alphabet_and_grammar_scm,
			       SExpr*& top_level_sexpr,
			       SExpr*& alphabet_and_grammar_sxpr)
{
  top_level_sexpr = scm_to_new_sexpr (alphabet_and_grammar_scm);
  alphabet_and_grammar_sxpr = &(*top_level_sexpr)[0];
}


static SCM xrate_estimate_tree (SCM stock_smob, SCM alphabet_and_grammar)
{
  SCM scm = SCM_BOOL_F;
  SExpr *sexpr = 0, *alphgram = 0;
  try {
    Stockholm_smob *stock = Stockholm_smob::cast_from_scm (stock_smob);
    get_alphgram_sexpr (alphabet_and_grammar, sexpr, alphgram);

    ECFG_main xrate;
    Stockholm stock_with_tree = xrate.run_tree_estimation (stock->stock, stock->seqdb, *alphgram);

    // make return expression
    scm = make_stockholm_smob (stock_with_tree);

  } catch (Dart_exception& e) {
    CLOGERR << e.what();
  }

  // cleanup and return
  if (sexpr)
    delete sexpr;

  return scm;
}

static SCM xrate_annotate_alignment (SCM stock_smob, SCM alphabet_and_grammar)
{
  SCM scm = SCM_BOOL_F;
  SExpr *sexpr = 0, *alphgram = 0;
  try {
    Stockholm_smob *stock = Stockholm_smob::cast_from_scm (stock_smob);
    get_alphgram_sexpr (alphabet_and_grammar, sexpr, alphgram);

    ECFG_main xrate;
    Stockholm stock_with_annotation = xrate.run_alignment_annotation (stock->stock, *alphgram);

    // make return expression
    scm = make_stockholm_smob (stock_with_annotation);

  } catch (Dart_exception& e) {
    CLOGERR << e.what();
  }

  // cleanup and return
  if (sexpr)
    delete sexpr;

  return scm;
}

static SCM xrate_train_grammar (SCM list_of_stock_smobs, SCM alphabet_and_grammar)
{
  SCM scm = SCM_BOOL_F;
  SExpr *sexpr = 0, *alphgram = 0;
  try {
    Stockholm_database stock_db;
    get_alphgram_sexpr (alphabet_and_grammar, sexpr, alphgram);

    /* Check that first argument is a list */ 
    if (SCM_NFALSEP(scm_list_p(list_of_stock_smobs)))
      {
	/* Iterate through the list */ 
	while (SCM_FALSEP (scm_null_p (list_of_stock_smobs)))
	  {
	    /* Get the head of the list */ 
	    SCM stock_smob = SCM_CAR(list_of_stock_smobs);

	    /* Add it to the Stockholm_database */
	    stock_db.add (Stockholm_smob::cast_from_scm(stock_smob)->stock);

	    /* Discard the head of the list */
	    list_of_stock_smobs = SCM_CDR(list_of_stock_smobs);
	  }
      }
    else   // first arg is not a list, so attempt to treat it as a single alignment
      stock_db.add (Stockholm_smob::cast_from_scm(list_of_stock_smobs)->stock);

    // do the training
    ECFG_main xrate;
    ECFG_scores* trained_grammar = 0;
    ECFG_counts* trained_counts = 0;
    xrate.run_grammar_training (stock_db, *alphgram, &trained_grammar, &trained_counts);

    // make return expression
    scm = ecfg_to_scm (*trained_grammar, trained_counts);

  } catch (Dart_exception& e) {
    CLOGERR << e.what();
  }

  // cleanup and return
  if (sexpr)
    delete sexpr;

  return scm;
}

static SCM xrate_validate_stockholm_grammar (SCM stock_smob, SCM alphabet_and_grammar)
{
  SCM scm = SCM_BOOL_F;
  SExpr *sexpr = 0, *alphgram = 0;
  try {
    Stockholm_smob *stock = Stockholm_smob::cast_from_scm (stock_smob);
    get_alphgram_sexpr (alphabet_and_grammar, sexpr, alphgram);

    ECFG_main xrate;
    ECFG_scores* ecfg = xrate.run_macro_expansion (stock->stock, *alphgram);

    // make return expression
    scm = ecfg_to_scm (*ecfg, 0);

  } catch (Dart_exception& e) {
    CLOGERR << e.what();
  }

  // cleanup and return
  if (sexpr)
    delete sexpr;

  return scm;
}

static SCM xrate_validate_grammar (SCM alphabet_and_grammar)
{
  SCM scm = SCM_BOOL_F;
  SExpr *sexpr = 0, *alphgram = 0;
  try {
    get_alphgram_sexpr (alphabet_and_grammar, sexpr, alphgram);

    ECFG_main xrate;
    ECFG_scores* ecfg = xrate.run_macro_expansion (*alphgram);

    // make return expression
    scm = ecfg_to_scm (*ecfg, 0);

  } catch (Dart_exception& e) {
    CLOGERR << e.what();
  }

  // cleanup and return
  if (sexpr)
    delete sexpr;

  return scm;
}

SCM ecfg_to_scm (const ECFG_scores& ecfg, const ECFG_counts* counts)
{
  sstring grammar_alphabet_string;
  grammar_alphabet_string << '(';
  ECFG_builder::ecfg2stream (grammar_alphabet_string, ecfg.alphabet, ecfg, counts);
  ECFG_builder::alphabet2stream (grammar_alphabet_string, ecfg.alphabet);
  grammar_alphabet_string << ')';
  return string_to_scm (grammar_alphabet_string.c_str());
}

// main guile initialization routine
void init_xrate_primitives (void)
{
  scm_c_define_gsubr ("xrate-validate-grammar", 1, 0, 0, (SCM (*)()) xrate_validate_grammar);
  scm_c_define_gsubr ("xrate-validate-stockholm-grammar", 2, 0, 0, (SCM (*)()) xrate_validate_stockholm_grammar);
  scm_c_define_gsubr ("xrate-estimate-tree", 2, 0, 0, (SCM (*)()) xrate_estimate_tree);
  scm_c_define_gsubr ("xrate-annotate-alignment", 2, 0, 0, (SCM (*)()) xrate_annotate_alignment);
  scm_c_define_gsubr ("xrate-train-grammar", 2, 0, 0, (SCM (*)()) xrate_train_grammar);
}
