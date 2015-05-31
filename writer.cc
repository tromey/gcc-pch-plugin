#include "writer.hh"

hash_writer::patcher::patcher (hash_writer *writer, tree t)
  : m_writer (writer)
{
  m_writer->m_patchers.push_back (this);

  int token = - (1 + m_writer->m_patchers.size());
  m_writer->objects[t] = token;
}

hash_writer::patcher::~patcher ()
{
  assert (m_writer->m_patchers.back () == this);
  m_writer->m_patchers.pop_back ();

  ssize_t val = m_writer->here ();
  for (auto iter = m_patches.begin (); iter != m_patches.end (); ++iter)
    m_writer->emit_at (*iter, val);
}

hash_writer::hash_writer (const char *plugin_name, const char *filename)
  : m_filename (filename)
{
  register_callback (plugin_name, PLUGIN_GGC_MARKING, exported_mark, this);

  register_callback (plugin_name, PLUGIN_FINISH_TYPE, exported_add, this);
  register_callback (plugin_name, PLUGIN_FINISH_DECL, exported_add, this);
  // Note that PLUGIN_FINISH_UNIT is not called with --syntax-only.
  register_callback (plugin_name, PLUGIN_FINISH, exported_finish, this);
}

void
hash_writer::mark ()
{
  for (auto iter = inputs.begin (); iter != inputs.end (); ++iter)
    ggc_mark (*iter);
  for (auto iter = objects.begin (); iter != objects.end (); ++iter)
    ggc_mark ((*iter).first);
}

/* static */ void
hash_writer::exported_mark (void *, void *self)
{
  assert (self != nullptr);
  hash_writer *writer = static_cast<hash_writer *>(self);
  writer->mark ();
}

void
hash_writer::add (tree t)
{
  if ((TYPE_P (t) && TYPE_NAME (t))
      || (DECL_P (t) && DECL_NAME (t)
	  && (TREE_CODE (t) == FUNCTION_DECL
	      || TREE_CODE (t) == VAR_DECL
	      || TREE_CODE (t) == TYPE_DECL)))
    inputs.push_back (t);
}

/* static */ void
hash_writer::exported_add (void *vt, void *self)
{
  assert (self != nullptr);
  tree t = static_cast<tree> (vt);
  hash_writer *writer = static_cast<hash_writer *> (self);
  writer->add (t);
}

void
hash_writer::do_fwrite (FILE *out, ssize_t val)
{
  char buf[4];

  for (int i = 0; i < 4; ++i)
    {
      buf[i] = val & 0xff;
      val >>= 8;
    }

  fwrite (buf, 4, 1, out);
}

void
hash_writer::do_fwrite (FILE *out, const char *data, size_t len)
{
  fwrite (data, len, 1, out);
}

void
hash_writer::finish ()
{
  if (inputs.empty ())
    return;

  FILE *out = fopen (m_filename.c_str (), "w");
  do_fwrite (out, 1); // FIXME version

  for (int i = 0; i < 2; ++i)
    {
      for (auto iter = inputs.begin (); iter != inputs.end (); ++iter)
	{
	  if (!(i == 0 ? DECL_P (*iter) : TYPE_P (*iter)))
	    continue;

	  ssize_t pool_off = get (*iter);
	  tree name = DECL_P (*iter) ? DECL_NAME (*iter) : TYPE_NAME (*iter);
	  do_fwrite (out, IDENTIFIER_POINTER (name),
		     IDENTIFIER_LENGTH (name) + 1);
	  do_fwrite (out, pool_off);
	}

      do_fwrite (out, "", 1);
    }

  do_fwrite (out, m_buffer, m_offset);
  fclose (out);
}

/* static */ void
hash_writer::exported_finish (void *, void *self)
{
  assert (self != nullptr);
  hash_writer *writer = static_cast<hash_writer *> (self);
  writer->finish ();
}

ssize_t
hash_writer::write_int_type (tree t)
{
  ssize_t result = here ();
  emit ('i');
  ssize_t size = int_size_in_bytes (t);
  if (!TYPE_UNSIGNED (t))
    size = -size;
  emit (size);
  return result;
}

ssize_t
hash_writer::write_float_type (tree t)
{
  ssize_t result = here ();
  emit ('f');
  emit (static_cast<ssize_t> (int_size_in_bytes (t)));
  return result;
}

ssize_t
hash_writer::write_pointer_type (tree t)
{
  ssize_t base = get (TREE_TYPE (t));
  ssize_t result = here ();
  emit ('p');
  emit (base);
  return result;
}

ssize_t
hash_writer::write_qualified_type (tree t)
{
  ssize_t base = get (build_qualified_type (t, 0));
  ssize_t result = here ();
  emit ('q');
  emit (static_cast<ssize_t> (TYPE_QUALS (t)));
  emit (base);
  return result;
}

ssize_t
hash_writer::write_bitfield_type (tree t)
{
  ssize_t result = here ();
  emit (':');
#if fixme
  emit (FIXME);
#endif
  return result;
}

ssize_t
hash_writer::write_array_type (tree t)
{
  ssize_t element = get (TREE_TYPE (t));

  ssize_t result = here ();
  emit ('[');
  ssize_t len = tree_to_shwi (TYPE_MAX_VALUE (TYPE_DOMAIN (t))) + 1;
  emit (len);
  emit (element);
  return result;
}

ssize_t
hash_writer::write_enum_type (tree t)
{
  ssize_t n_csts = 0;
  for (tree iter = TYPE_VALUES (t); iter; iter = TREE_CHAIN (iter))
    ++n_csts;

  ssize_t result = here ();
  emit ('e');
  emit (n_csts);
  for (tree iter = TYPE_VALUES (t); iter; iter = TREE_CHAIN (iter))
    {
      emit (IDENTIFIER_POINTER (TREE_PURPOSE (iter)));
#if fixme
      emit (fixme_value (TREE_VALUE (iter)));
#endif
    }
  return result;
}

ssize_t
hash_writer::write_function_type (tree t)
{
  // Make sure all argument types have been emitted.
  ssize_t n_args = 0;
  ssize_t is_varargs = true;
  for (tree iter = TYPE_ARG_TYPES (t); iter; iter = TREE_CHAIN (iter))
    {
      if (iter == void_type_node)
	{
	  assert (TREE_CHAIN (iter) == NULL_TREE);
	  is_varargs = false;
	  break;
	}
      ++n_args;
      get (TREE_VALUE (iter));
    }

  ssize_t ret_type = get (TREE_TYPE (t));

  ssize_t result = here ();
  emit ('(');
  emit (ret_type);
  emit (n_args);
  emit (is_varargs);
  for (tree iter = TYPE_ARG_TYPES (t); iter; iter = TREE_CHAIN (iter))
    emit (get (TREE_VALUE (iter)));

  return result;
}

ssize_t
hash_writer::write_struct_or_union_type (tree t)
{
  ssize_t n_elts = 0;

  {
    patcher hold_patcher (this, t);

    for (tree iter = TYPE_FIELDS (t); iter; iter = TREE_CHAIN (iter))
      {
	assert (TREE_CODE (iter) == FIELD_DECL);
	++n_elts;
	get (TREE_TYPE (iter));
      }
  }

  ssize_t result = here ();
  emit (TREE_CODE (t) == RECORD_TYPE ? '{' : '|');
  emit (n_elts);

  for (tree iter = TYPE_FIELDS (t); iter; iter = TREE_CHAIN (iter))
    {
      emit (IDENTIFIER_POINTER (DECL_NAME (iter)));
      emit (get (TREE_TYPE (iter)));
    }

  return result;
}

ssize_t
hash_writer::write_decl (tree t)
{
  ssize_t type = get (TREE_TYPE (t));

  ssize_t result = here ();
  emit ('S');
  emit (TREE_CODE (t) == FUNCTION_DECL ? 'f'
	 : (TREE_CODE (t) == VAR_DECL ? 'v' : 't'));
  emit (IDENTIFIER_POINTER (DECL_NAME (t)));
  emit (type);
  return result;
}

ssize_t
hash_writer::write (tree t)
{
  if (TYPE_P (t) && TYPE_QUALS (t))
    return write_qualified_type (t);

  switch (TREE_CODE (t))
    {
    case INTEGER_TYPE:
      return write_int_type (t);
    case REAL_TYPE:
      return write_float_type (t);
    case POINTER_TYPE:
      return write_pointer_type (t);
    case FUNCTION_TYPE:
      return write_function_type (t);
    case ARRAY_TYPE:
      return write_array_type (t);
    case ENUMERAL_TYPE:
      return write_enum_type (t);
    // FIXME bitfield
    case UNION_TYPE:
    case RECORD_TYPE:
      return write_struct_or_union_type (t);

    case FUNCTION_DECL:
    case VAR_DECL:
    case TYPE_DECL:
      return write_decl (t);

    case VOID_TYPE:
      break;

    default:
      fprintf (stderr, "[tree code %d]\n", int (TREE_CODE (t)));
      abort ();
    }
}

ssize_t
hash_writer::get (tree t)
{
  auto ptr = objects.find (t);
  if (ptr != objects.end ())
    return (*ptr).second;
  ssize_t result = write (t);
  objects[t] = result;
  return result;
}

void
hash_writer::emit (char c)
{
  emit (&c, 1);
}

void
hash_writer::emit (const char *s)
{
  emit (s, strlen (s) + 1);
}

void
hash_writer::emit (ssize_t val)
{
  char buf[4];

  if (val < 0)
    {
      ssize_t h = here ();
      ssize_t index = -(val + 1);
      m_patchers[index]->note_patch (here ());
    }

  for (int i = 0; i < 4; ++i)
    {
      buf[i] = val & 0xff;
      val >>= 8;
    }

  emit (buf, 4);
}

void
hash_writer::emit_at (size_t offset, ssize_t val)
{
  char buf[4];

  for (int i = 0; i < 4; ++i)
    {
      buf[i] = val & 0xff;
      val >>= 8;
    }

  memcpy (m_buffer + offset, buf, 4);
}

void
hash_writer::emit (const char *data, size_t len)
{
  if (m_offset + len >= m_len)
    {
      m_len *= 2;
      if (m_offset + len >= m_len)
	m_len = m_offset + len;
      m_buffer = static_cast<char *> (xrealloc (m_buffer, m_len));
    }

  memcpy (m_buffer + m_offset, data, len);
  m_offset += len;
}
