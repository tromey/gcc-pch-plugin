#include <stdio.h>

class fclose_deleter
{
public:

  void operator () (FILE *f)
  {
    if (f)
      fclose (f);
  }
};
