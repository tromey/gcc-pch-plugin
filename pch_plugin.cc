#include "pch_plugin.hh"
#include "readhash.hh"
#include "writer.hh"
#include "c-family/c-pragma.h"
#include "toplev.h"
#include "plugin-version.h"

pch_plugin::pch_plugin(const char *plugin_name)
{
  assert (singleton == nullptr);
  singleton = this;
  c_binding_oracle = exported_binding_oracle;

  register_callback (plugin_name, PLUGIN_PRAGMAS, init_pragmas, nullptr);
  register_callback (plugin_name, PLUGIN_GGC_MARKING, exported_mark, NULL);
}

void
pch_plugin::binding_oracle (c_oracle_request kind, tree identifier)
{
  const char *name = IDENTIFIER_POINTER (identifier);

  for (auto iter = maps.begin (); iter != maps.end (); ++iter)
    {
      tree result = (*iter)->find (kind, name);
      if (result != NULL_TREE)
	{
	  if (kind == C_ORACLE_SYMBOL)
	    {
	      c_bind (BUILTINS_LOCATION /* FIXME */, result, 1);
	      rest_of_decl_compilation (result, 1, 0);
	    }
	  else
	    {
	      assert (kind == C_ORACLE_TAG);
	      c_pushtag (BUILTINS_LOCATION /* FIXME */, identifier, result);
	    }
	  // Perhaps instead we should search for and reject
	  // duplicates here.
	  break;
	}
    }
}

/* static */ void
pch_plugin::exported_binding_oracle (c_oracle_request kind, tree identifier)
{
  assert (singleton != nullptr);
  singleton->binding_oracle (kind, identifier);
}

size_t
pch_plugin::read_file (const char *filename, char **data)
{
  struct stat sbuf;
  FILE *f = fopen (filename, "r");
  if (f == NULL)
    return 0;
  if (fstat (fileno (f), &sbuf) < 0)
    {
      fclose (f);
      return 0;
    }
  *data = new char[sbuf.st_size];
  if (fread (*data, 1, sbuf.st_size, f) != sbuf.st_size)
    {
      delete[] *data;
      fclose (f);
      return 0;
    }

  fclose (f);
  return sbuf.st_size;
}

void
pch_plugin::pragma_import_pch ()
{
  tree value;
  cpp_ttype type = pragma_lex (&value);

  if (type == CPP_STRING)
    {
      // If we wanted to be tricky we could read the file in a
      // separate thread.  This would require just a tiny bit of
      // locking to present a consistent view to the C parser.
      char *data;
      size_t len = read_file (TREE_STRING_POINTER (value), &data);
      if (!len)
	{
	  // FIXME error.
	}
      else
	{
	  mapped_hash *hash = new mapped_hash ((const uint8_t *) data, len);
	  if (!hash->init ())
	    delete hash;
	  else
	    maps.push_back (hash);
	}
    }
  else
    {
      // FIXME error
    }
}

/* static */ void
pch_plugin::exported_pragma_import_pch (cpp_reader *)
{
  assert (singleton != nullptr);
  singleton->pragma_import_pch ();
}

/* static */ void
pch_plugin::init_pragmas (void *, void *)
{
  c_register_pragma ("GCC", "import_pch", exported_pragma_import_pch);
}

void
pch_plugin::mark ()
{
  for (auto iter = maps.begin (); iter != maps.end (); ++iter)
    (*iter)->mark ();
}

/* static */ void
pch_plugin::exported_mark (void *, void *)
{
  assert (singleton != nullptr);
  singleton->mark ();
}

#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif

int
plugin_init (struct plugin_name_args *plugin_info,
	     struct plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version))
    return 1;

  for (int i = 0; i < plugin_info->argc; ++i)
    {
      if (strcmp (plugin_info->argv[i].key, "output") == 0)
	{
	  new hash_writer (plugin_info->base_name, plugin_info->argv[i].value);
	  break;
	}
    }

  // Called for side effects.  So awful.
  new pch_plugin(plugin_info->base_name);

  return 0;
}
