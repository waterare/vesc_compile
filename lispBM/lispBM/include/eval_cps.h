/*
    Copyright 2018, 2020, 2021, 2022 Joel Svensson  svenssonjoel@yahoo.se

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/** \file eval_cps.h */
#ifndef EVAL_CPS_H_
#define EVAL_CPS_H_

#include "stack.h"
#include "lispbm_types.h"

#define EVAL_CPS_STATE_INIT    0
#define EVAL_CPS_STATE_PAUSED  1
#define EVAL_CPS_STATE_RUNNING 2
#define EVAL_CPS_STATE_STEP    3
#define EVAL_CPS_STATE_KILL    4

/** The eval_context_t struct represents a lispbm process.
 *
 */
typedef struct eval_context_s{
  lbm_value program;
  lbm_value curr_exp;
  lbm_value curr_env;
  lbm_value mailbox;  /*massage passing mailbox */
  lbm_value r;
  bool  done;
  bool  app_cont;
  lbm_stack_t K;
  /* Process control */
  uint32_t timestamp;
  uint32_t sleep_us;
  lbm_cid id;
  /* List structure */
  struct eval_context_s *prev;
  struct eval_context_s *next;
} eval_context_t;

/** A function pointer type to use together with the queue iterators.
 *
 * \param
 * \param
 * \param
 */
typedef void (*ctx_fun)(eval_context_t *, void*, void*);

/* Common interface */
/** Get the global environment
 *
 * \return global environment.
 */
extern lbm_value eval_cps_get_env(void);

/* Concurrent interface */
/** Initialize the evaluator.
 *
 * \return 1 on success and 0 on failure.
 */
extern int lbm_eval_init(void);
/** Remove a context that has finished executing and free up its associated memory.
 *
 * \param cid Context id of context to free.
 * \param v Pointer to an lbm_value to store the result computed by the program into.
 * Note that for comples values, GC will free these up the next time it runs.
 * \return 1 if a context was successfully removed otherwise 0.
 */
extern int lbm_remove_done_ctx(lbm_cid cid, lbm_value *v);
/** Wait for a context to appear in the done queue. This function is
 * dangerous if called with a context id of a process that will never finish.
 * In such a situation this function will not return.
 *
 * \param cid Context id to wait for.
 * \return Result computed by the program running in the context.
 */
extern lbm_value lbm_wait_ctx(lbm_cid cid);

/** Creates a context and initializes it with the provided program. The context
 * is added to the ready queue and will start executing when the evaluator is free.
 *
 * \param lisp Program to launch
 * \return a context id on success and 0 on failure to launch a context.
 */
extern lbm_cid lbm_eval_program(lbm_value lisp);
/** An extended version of lbm_eval_program that allows specification of stack size to use.
 *
 * \param lisp Program to launch.
 * \param stack_size Size of the continuation stack for this context.
 * \return a context id on success and 0 on failure to launch a context.
 */
extern lbm_cid lbm_eval_program_ext(lbm_value lisp, unsigned int stack_size);

/** This function executes the evaluation loop and does not return.
 *  lbm_run_eval should be started in a new thread provided by the underlying HAL or OS.
 */
extern void lbm_run_eval(void);

/** Indicate that the evaluator should pause at the next iteration.
 * You cannot assume that the evaluator has paused unless you call lbm_get_eval_state and get the
 * return value EVAL_CPS_STATE_PAUSED.
 */
extern void lbm_pause_eval(void);
/** Perform a single step of evaluation.
 * The evaluator should be in EVAL_CPS_STATE_PAUSED before running this function.
 * After taking one step of evaluation, the evaluator will return to being in the
 * EVAL_CPS_STATE_PUASED state.
 */
extern void lbm_step_eval(void);
/** Resume from being in EVAL_CPS_STATE_PAUSED.
 *
 */
extern void lbm_continue_eval(void);
/** This will kill the evaluator on the next iteration.
 *
 */
extern void lbm_kill_eval(void);
/** Get the current state of the evaluator.
 *
 * \return Current state of the evaluator.
 */
extern uint32_t lbm_get_eval_state(void);

/* statistics interface */
/**  Iterate over all ready contexts and apply function on each context.
 *
 * \param f Function to apply to each context.
 * \param arg1 Pointer argument that can be used to convey information back to user
 * \param arg2 Same as above.
 */
extern void lbm_running_iterator(ctx_fun f, void*, void*);
/** Iterate over all blocked contexts and apply function on each context.
 *
 * \param f Function to apply to each context.
 * \param arg1 Pointer argument that can be used to convey information back to user
 * \param arg2 Same as above
 */
extern void lbm_blocked_iterator(ctx_fun f, void*, void*);
/** Iterate over all done contexts and apply function on each context.
 *
 * \param f Function to apply to each context.
 * \param arg1 Pointer argument that can be used to convey information back to user
 * \param arg2 Same as above
 */
extern void lbm_done_iterator(ctx_fun f, void*, void*);

/*
  Callback routines for sleeping and timestamp generation.
  Depending on target platform these will be implemented in different ways.
  Todo: It may become necessary to also add a mutex callback.
*/
/** Set a usleep callback for use by the evaluator thread.
 *
 * \param fptr Pointer to a sleep function.
 */
extern void lbm_set_usleep_callback(void (*fptr)(uint32_t));
/** Set a timestamp callback for use by the evaluator thread.
 *
 * \param fptr Pointer to a timestamp generating function.
 */
extern void lbm_set_timestamp_us_callback(uint32_t (*fptr)(void));
/** Set a "done" callback function. This function will be called by
 * the evaluator when a context finishes execution.
 *
 * \param fptr Pointer to a "done" function.
 */
extern void lbm_set_ctx_done_callback(void (*fptr)(eval_context_t *));

/* loading of programs interface */
/** Load and schedule a program for execution.
 *
 * \param tokenizer The tokenizer to read the program from.
 * \return A context id on success or 0 on failure.
 */
extern lbm_cid lbm_load_and_eval_program(lbm_tokenizer_char_stream_t *tokenizer);
/** Load and schedule an expression for execution.
 *
 * \param tokenizer The tokenizer to read the expression from.
 * \return A context id on success or 0 on failure.
 */
extern lbm_cid lbm_load_and_eval_expression(lbm_tokenizer_char_stream_t *tokenizer);
/** Load a program and bind it to a symbol in the environment.
 *
 * \param tokenizer The tokenizer to read the program from.
 * \param symbol A string with the name you want the binding to have in the environment.
 * \return A context id on success or 0 on failure.
 */
extern lbm_cid lbm_load_and_define_program(lbm_tokenizer_char_stream_t *tokenizer, char *symbol);
/** Load an expression and bind it to a symbol in the environment.
 *
 * \param tokenizer The tokenizer to read the expression from.
 * \param symbol A string with the name you want the binding to have in the environment.
 * \return A context id on success or 0 on failure.
 */
extern lbm_cid lbm_load_and_define_expression(lbm_tokenizer_char_stream_t *tokenizer, char *symbol);

/* Evaluating a definition in a new context */
/** Create a context for a bound expression and schedule it for execution
 *
 * \param symbol The name of the binding to schedule for execution.
 * \return A context if on success or 0 on failure.
 */
extern lbm_cid lbm_eval_defined_expression(char *symbol);
/** Create a context for a bound program and schedule it for execution
 *
 * \param symbol The name of the binding to schedule for execution.
 * \return A context if on success or 0 on failure.
 */
extern lbm_cid lbm_eval_defined_program(char *symbol);

/* send message from c to LBM process */
/** Send a message to a process running in the evaluator.
 *
 * \param cid Context id of the process to send a message to.
 * \param msg lbm_value that will be sent to the process.
 * \return 1 on success or 0 on failure.
 */
extern int lbm_send_message(lbm_cid cid, lbm_value msg);
#endif