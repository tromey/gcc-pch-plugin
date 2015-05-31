
void
pushdecl_safe (tree decl)
{
  c_binding_oracle_function *save = c_binding_oracle;
  c_binding_oracle = NULL;
  pushdecl (decl);
  c_binding_oracle = save;
}
