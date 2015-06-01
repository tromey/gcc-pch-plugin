This is an implementation of pre-compiled headers for GCC, written as
a GCC plugin.

This approach is better than the approach that is supplied with GCC
itself.  Multiple PCH files can be used at once, performance should be
better, and it is not based on writing out a heap image.

## Building

You will need a very new version of GCC -- one that supports the GDB
`compile` feature.  The plugin uses the same hooks in GCC to do its
work.

Once you have a new-enough GCC, you should be able to build the plugin
with something like:

```
make I=
```

Here, `I` is the prefix under which GCC is installed.  The above
invocation would look for `/bin/gcc`.

## Use

First, compile a header file with the plugin to create a PCH file.
You can do that like:

```
gcc --syntax-only \
  -fplugin=.../libcphplugin.so \
  -fplugin-arg-libpch-plugin-output=something.npch \
  testfile.h
```

This will create a new "something.npch" file.  This files holds all
the information discovered while parsing the sample file.

Now in your source you can import the `.npch` file using a pragma:

```
#pragma GCC pch_import "something.npch"
```

You must still make sure the plugin is loaded, so invoke like:

```
gcc -fplugin=.../libcphplugin.so ... testfile.c
```

## Performance

I did a simple test using `<gtk/gtk.h>`.

Parsing this file took 0.3 seconds, but loading the `.npch` file
took 0.03 seconds.

## Inner Workings

On the writing side, the plugin simple notices new types and
declarations and writes them to the output file, using a simple
serialization scheme.

On the reading side, the plugin uses the "C binding oracle" that was
added to GCC for use the GDB `compile` plugin.  This oracle is called
whenever the C front end needs the definition of a symbol -- and the
plugin looks in its database to see if a definition exists.

The plugin is written to lazily instantiate symbols and types.  That
is, if a symbol is not used in the current compilation, then the
corresponding GCC tree structures will never be instantiated.  This is
where the plugin gets its performance improvement.

## Limitations and To-Do

* Macros.  The plugin ignores macros, but of course this is wrong.
  The best way to handle this would be to add a "macro oracle" to
  libcpp, so that macros could also be lazily instantiated.

* Inline functions.  The plugin does not serialize inline function
  bodies.  This is doable, but would require more work.  I think a
  good way to go would be to try to reuse parts of the LTO streamer.

* Lesser-used C features aren't supported.  I didn't bother with VLAs,
  or vectors, or complex numbers.  In fact the current code has a few
  FIXMEs around even common features like bitfields.

* C++.  The potential win from an improved PCH is bigger with C++ than
  with C.  Right now there isn't anything like the binding oracle for
  the C++ compiler (and it isn't even clear this is the way it should
  work there).  C++ can be revisited once the GDB `compile` support
  for it has been written.
