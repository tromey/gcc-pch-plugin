// blah

#ifndef NPCH_PCH_PLUGIN_HH
#define NPCH_PCH_PLUGIN_HH

#include "gcc-plugin.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "c-tree.h"
#include <assert.h>
#include <list>

class cpp_reader;
class mapped_hash;

class pch_plugin
{
public:

  pch_plugin (const char *plugin_name);

  ~pch_plugin ()
  {
  }

private:

  size_t read_file (const char *filename, char **data);

  void binding_oracle (c_oracle_request, tree);
  static void exported_binding_oracle (c_oracle_request, tree);

  void pragma_import_pch ();
  static void exported_pragma_import_pch (cpp_reader *);

  void mark ();
  static void exported_mark (void *, void *);

  static void init_pragmas (void *, void *);

  static pch_plugin *singleton;

  std::list<mapped_hash *> maps;
};

#endif // NPCH_PCH_PLUGIN_HH
