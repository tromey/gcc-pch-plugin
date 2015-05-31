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


  ssize_t write_int_type (tree);
  ssize_t write_float_type (tree);
  ssize_t write_pointer_type (tree);
  ssize_t write_qualified_type (tree);
  ssize_t write_bitfield_type (tree);
  ssize_t write_array_type (tree);
  ssize_t write_enum_type (tree);
  ssize_t write_function_type (tree);
  ssize_t write_struct_or_union_type (tree);
  ssize_t write_void_type ();
  ssize_t write_decl (tree);
  ssize_t write (tree);
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

  void do_fwrite (FILE *, ssize_t);
  void do_fwrite (FILE *, const char *, size_t);

  class patcher
  {
  public:
    patcher (hash_writer *, tree);
    ~patcher ();

    void note_patch (ssize_t offset)
    {
      m_patches.push_back (offset);
    }

  private:

    std::list<ssize_t> m_patches;
    hash_writer *m_writer;
  };

  std::string m_filename;
  std::list<tree> inputs;
  std::unordered_map<tree, ssize_t> objects;
  std::vector<patcher *> m_patchers;

  char *m_buffer;
  size_t m_offset;
  size_t m_len;
};

#endif // NPCH_WRITER_HH
