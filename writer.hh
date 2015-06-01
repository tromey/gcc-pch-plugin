#ifndef NPCH_WRITER_HH
#define NPCH_WRITER_HH

#include "gcc-plugin.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include <stdlib.h>
#include <list>
#include <vector>
#include <string>
#include <unordered_map>
#include "ggc.h"
#include <assert.h>

class hash_writer
{
public:

  hash_writer (const char *, const char *);

  ~hash_writer ()
  {
  }

private:

  void add (tree);
  static void exported_add (void *, void *);

  void mark ();
  static void exported_mark (void *, void *);

  void finish ();
  static void exported_finish (void *, void *);


  void write_int_type (tree);
  void write_float_type (tree);
  void write_pointer_type (tree);
  void write_qualified_type (tree);
  void write_bitfield_type (tree);
  void write_array_type (tree);
  void write_enum_type (tree);
  void write_function_type (tree);
  void write_struct_or_union_type (tree);
  void write_void_type ();
  void write_decl (tree);
  void write (tree);
  ssize_t get (tree);

  size_t here ()
  {
    return m_offset;
  }

  // FIXME - error handling.
  void emit (char);
  void emit (ssize_t);
  void emit (const char *);
  void emit (const char *, size_t);
  void emit_at (size_t, ssize_t);
  void ensure (size_t);

  void do_fwrite (FILE *, ssize_t);
  void do_fwrite (FILE *, const char *, size_t);

  std::string m_filename;
  std::list<tree> inputs;
  std::unordered_map<tree, ssize_t> objects;

  char *m_buffer;
  size_t m_offset;
  size_t m_len;
};

#endif // NPCH_WRITER_HH
