/* itset.h - Inferior/Thread sets.
   Copyright (C) 2010, 2011 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef ITSET_H
#define ITSET_H

struct inferior;
struct thread_info;
struct ada_task_info;
struct cleanup;

/* This is an opaque type representing an I/T set.  An I/T set is
   simply a set of inferiors and/or threads.  A set may be dynamic
   (the members are enumerated at the time of use) or static (the
   members are enumerated at the time of construction); but this
   distinction is hidden from the callers.  An I/T set object is
   reference counted.  */

struct itset;
struct named_itset;

enum itset_width
{
  /* Sorted by increasing order.  Needed for itset_set_get_width.  */
  ITSET_WIDTH_DEFAULT,
  ITSET_WIDTH_THREAD,
  ITSET_WIDTH_ADA_TASK,
  ITSET_WIDTH_INFERIOR,
  ITSET_WIDTH_GROUP,
  ITSET_WIDTH_EXPLICIT,
  ITSET_WIDTH_ALL,

  ITSET_WIDTH_MIXED,
};

/* Create a new I/T set from a user specification.  The valid forms of
   a specification are documented in the manual.  *SPEC is the input
   specification, and it is updated to point to the first non-space
   character after the end of the specification.  */

struct itset *itset_create (char **spec);

/* Const version of the above.  */

struct itset *itset_create_const (const char **spec,
				  enum itset_width default_width);

struct itset *itset_create_spec (const char *spec);

/* Create a new I/T set based on TEMPLATE with ITSET_DEFAULT_WIDTH
   width replaced by DEFAULT_WIDTH.  */

struct itset *itset_clone_replace_default_width
  (const struct itset *template,
   enum itset_width default_width);

/* Add ADDME to the I/T set TO.  In other words, after the call, TO
   will be the union set of TO at entry, and ADDME.  */

void itset_add_set (struct itset *to, struct itset *addme);

/* Create an empty I/T set.  Usually, an empty set is the set that
   matches nothing.  In some contexts, though, an empty set represents
   some default.  */

struct itset *itset_create_empty (void);

/* Returns true if SET is the empty set.  That is a set whose spec is
   either "" or "empty".  */

int itset_is_empty_set (struct itset *set);

/* Returns true if ITSET contains any thread.  */

int itset_contains_any_thread (struct itset *itset);

/* Create a new dynamic I/T set which represents the current inferior,
   at the time the I/T set if consulted.  */

struct itset *itset_create_current (void);

/* Like itset_create, but if *SPEC does not appear to be the start of
   an I/T set, it will call itset_create_current and return the
   result.  */

struct itset *itset_create_or_default (char **spec);

/* Return true if PSPACE is contained in the I/T set.  */

int itset_contains_program_space (struct itset *itset,
				  enum itset_width default_width,
				  struct program_space *pspace);

/* Return true if the inferior is contained in the I/T set.  */

int itset_contains_inferior (struct itset *itset, struct inferior *inf);

int itset_width_contains_inferior (struct itset *itset,
				   enum itset_width default_width,
				   struct inferior *inf);

/* Return true if the thread is contained in the I/T set.  */

int itset_width_contains_thread (struct itset *itset,
				 enum itset_width default_width,
				 struct thread_info *thr);

/* Return true if the thread is contained in the I/T set.  */

int itset_contains_thread (struct itset *itset,
			   struct thread_info *thr);

int itset_contains_thread_maybe_width (struct itset *set,
				       enum itset_width default_width,
				       struct thread_info *thr,
				       int including_width);

/* Return true if the Ada task is contained in the I/T set.  */

int itset_contains_ada_task (struct itset *set,
			     enum itset_width default_width,
			     const struct ada_task_info *task,
			     int including_width);

/* Return a pointer to the I/T set's name.  Unnamed I/T sets have a
   NULL name.  */

const char *itset_name (const struct itset *itset);

/* Return a pointer to the I/T set's spec.  */

const char *itset_spec (const struct itset *itset);

/* Acquire a new reference to an I/T set.  Returns the I/T set, for
   convenience.  */

struct itset *itset_reference (struct itset *itset);

/* Release a reference to an I/T set.  */

void itset_free (struct itset *itset);

struct cleanup *make_cleanup_itset_free (struct itset *itset);

/* A cleanup function that calls itset_free.  */

void itset_cleanup (void *itset);

/* Like iterate_over_inferiors, but iterate over only those inferiors
   in ITSET.  */

typedef int (itset_inf_callback_func) (struct inferior *, void *);
struct inferior *iterate_over_itset_inferiors (struct itset *itset,
					       enum itset_width default_width,
					       itset_inf_callback_func *callback,
					       void *data);

/* Iterate over all defined named itsets.  */

typedef void (iterate_over_named_itsets_ftype) (struct named_itset *, void *);
void
iterate_over_named_itsets (iterate_over_named_itsets_ftype func, void *data);

extern int named_itset_is_builtin (struct named_itset *itset);
extern int named_itset_number (struct named_itset *itset);
extern const char *named_itset_name (struct named_itset *itset);
extern struct itset *named_itset_set (struct named_itset *itset);
extern const char *named_itset_spec (struct named_itset *itset);

extern struct named_itset *find_named_itset (int num);

struct thread_info *
iterate_over_itset_threads (struct itset *itset,
			    enum itset_width default_width,
			    int (*callback) (struct thread_info *, void *),
			    void *datum);

/* The current I/T set.  */

extern struct itset *current_itset;

extern enum itset_width itset_get_width (struct itset *set);

extern void itfocus_from_thread_switch (void);
extern int itfocus_should_follow_stop_event (void);
extern struct thread_info *itset_get_toi (struct itset *set);
extern int itset_has_fixed_toi (struct itset *set);

#include "command.h"

/* Should probably move to infcmd.c, and the include above dropped.  */
extern void for_each_selected_thread_cmd (cmd_cfunc_ftype cmd,
					  char *args, int from_tty);

#endif /* ITSET_H */
