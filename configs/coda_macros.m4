AC_DEFUN(CODA_CC_FEATURE_TEST,
  [AC_CACHE_CHECK(whether the C compiler accepts $2, coda_cv_cc_$1,
     coda_saved_CC="$CC" ; CC="$CC $2" ; AC_LANG_SAVE
     AC_LANG_C
     AC_TRY_COMPILE([], [], coda_cv_cc_$1=yes, coda_cv_cc_$1=no)
     AC_LANG_RESTORE
     CC="$coda_saved_CC")
   if test $coda_cv_cc_$1 = yes; then
     CC="$CC $2"
   fi])

AC_DEFUN(CODA_CXX_FEATURE_TEST,
  [AC_CACHE_CHECK(whether the C++ compiler accepts $2, coda_cv_cxx_$1,
     coda_saved_CXX="$CXX" ; CXX="$CXX $2" ; AC_LANG_SAVE
     AC_LANG_CPLUSPLUS
     AC_TRY_COMPILE([], [], coda_cv_cxx_$1=yes, coda_cv_cxx_$1=no)
     AC_LANG_RESTORE
     CXX="$coda_saved_CXX")
   if test $coda_cv_cxx_$1 = yes; then
     CXX="$CXX $2"
   fi])

AC_SUBST(NATIVECC)
AC_DEFUN(CODA_PROG_NATIVECC,
    if test $cross_compiling = yes ; then
       [AC_CHECKING([for native C compiler on the build host])
	AC_CHECK_PROG(NATIVECC, gcc, gcc)
	if test -z "$NATIVECC" ; then
	    AC_CHECK_PROG(NATIVECC, cc, cc, , , /usr/ucb/cc)
	fi
	test -z "$NATIVECC" && AC_MSG_ERROR([no acceptable cc found in \$PATH])
	AC_MSG_RESULT([	found.])
	dnl Just assume it works.]
    else
	NATIVECC=${CC}
    fi)

dnl Check for typedefs in unusual headers
dnl Most of this function is identical to AC_CHECK_TYPE in autoconf-2.13
AC_DEFUN(CODA_CHECK_TYPE,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_type_$1,
[AC_EGREP_CPP(dnl
changequote(<<,>>)dnl
<<(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]>>dnl
changequote([,]), [#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#include <$3>], ac_cv_type_$1=yes, ac_cv_type_$1=no)])dnl
AC_MSG_RESULT($ac_cv_type_$1)
if test $ac_cv_type_$1 = no; then
  AC_DEFINE($1, $2, [Definition for $1 if the type is missing])
fi
])

dnl Check which lib provides termcap functionality
AC_SUBST(LIBTERMCAP)
AC_DEFUN(CODA_CHECK_LIBTERMCAP,
  AC_CHECK_LIB(termcap, main, [LIBTERMCAP="-ltermcap"],
    [AC_CHECK_LIB(ncurses, tgetent, [LIBTERMCAP="-lncurses"])]))

dnl Check for a curses library, and if it needs termcap
AC_SUBST(LIBCURSES)
AC_DEFUN(CODA_CHECK_LIBCURSES,
  if test $target != "i386-pc-cygwin32" -a $target != "i386-pc-djgpp" ; then
    AC_CHECK_LIB(ncurses, main, [LIBCURSES="-lncurses"],
	[AC_CHECK_LIB(curses, main, [LIBCURSES="-lcurses"],
	    [AC_MSG_ERROR("failed to find curses library")
	    ], $LIBTERMCAP)
	], $LIBTERMCAP)
    AC_CACHE_CHECK("if curses library requires -ltermcap",
	coda_cv_curses_needs_termcap,
	[coda_save_LIBS="$LIBS"
	LIBS="$LIBCURSES $LIBS"
	AC_TRY_LINK([],[],
	coda_cv_curses_needs_termcap=no,
	coda_cv_curses_needs_termcap=yes)
	LIBS="$coda_save_LIBS"])
    if test $coda_cv_curses_needs_termcap = yes; then
	LIBCURSES="$LIBCURSES $LIBTERMCAP"
    fi
  fi)


dnl Find an installed libdb-1.85
dnl  cygwin and glibc-2.0 have libdb
dnl  BSD systems have it in libc
dnl  glibc-2.1 has libdb1 (and libdb is db2)
dnl  Solaris systems have dbm in libc
dnl All coda-servers _must_ use the same library, otherwise certain databases
dnl cannot be replicated.
AC_SUBST(LIBDB)
AC_DEFUN(CODA_CHECK_LIBDB185,
  AC_CHECK_LIB(db, dbopen, [LIBDB="-ldb"],
    [AC_CHECK_LIB(c, dbopen, [LIBDB=""],
       [AC_CHECK_LIB(c, dbm_open, [],
          [if test $target != i386-pc-djgpp ; then
	     AC_MSG_ERROR("failed to find libdb")
	   fi])])])

  if test "$ac_cv_lib_c_dbm_open" = yes; then
    [AC_MSG_WARN([Found ndbm instead of libdb 1.85. This uses])
     AC_MSG_WARN([an incompatible disk file format and the programs])
     AC_MSG_WARN([will not be able to read replicated shared databases.])
     sleep 5
    AC_DEFINE(HAVE_NDBM, 1, [Define if you have ndbm])]
  else
    dnl Check if the found libdb is libdb2 using compatibility mode.
    AC_MSG_CHECKING("if $LIBDB is libdb 1.85")
    coda_save_LIBS="$LIBS"
    LIBS="$LIBDB $LIBS"
    AC_TRY_LINK([char db_open();], db_open(),
      [AC_MSG_RESULT("no")
       AC_CHECK_LIB(db1, dbopen, [LIBDB="-ldb1"],
         dnl It probably will compile but it wont be compatible..
         [AC_MSG_WARN([Found libdb2 instead of libdb 1.85. This uses])
          AC_MSG_WARN([an incompatible disk file format and the programs])
          AC_MSG_WARN([will not be able to read replicated shared databases.])])
	  sleep 5
       dnl In any case we should not be using db.h
       AC_DEFINE(HAVE_DB_185_H, 1, [Define if your libdb is 1.85])],
      [AC_MSG_RESULT("yes")])
    LIBS="$coda_save_LIBS"
  fi)

dnl check wether we have flock or fcntl
AC_DEFUN(CODA_CHECK_FILE_LOCKING,
  AC_CACHE_CHECK(for file locking by fcntl,
    fu_cv_lib_c_fcntl,
    [AC_TRY_COMPILE([#include <fcntl.h>
#include <stdio.h>], [ int fd; struct flock lk; fcntl(fd, F_SETLK, &lk);],
      fu_cv_lib_c_fcntl=yes,
      fu_cv_lib_c_fcntl=no)])
  if test $fu_cv_lib_c_fcntl = yes; then
    AC_DEFINE(HAVE_FCNTL_LOCKING, 1, [Define if you have fcntl file locking])
  fi

  AC_CACHE_CHECK(for file locking by flock,
    fu_cv_lib_c_flock,
    [AC_TRY_COMPILE([#include <sys/file.h>
#include <stdio.h>], [ int fd; flock(fd, LOCK_SH);],
      fu_cv_lib_c_flock=yes,
      fu_cv_lib_c_flock=no)])
  if test $fu_cv_lib_c_flock = yes; then
    AC_DEFINE(HAVE_FLOCK_LOCKING, 1, [Define if you have flock file locking])
  fi

  if test $fu_cv_lib_c_flock = no -a $fu_cv_lib_c_fcntl = no; then
         AC_MSG_ERROR("failed to find flock or fcntl")
  fi)

dnl check wether bcopy is defined in strings.h
AC_DEFUN(CODA_CHECK_BCOPY, 
  AC_CACHE_CHECK(for bcopy in strings.h,
    fu_cv_lib_c_bcopy,  
    [AC_TRY_COMPILE([#include <stdlib.h>
#include <strings.h>], 
      [ char *str; str = (char *) malloc(5); (void) bcopy("test", str, 5);],
      fu_cv_lib_c_bcopy=yes,
      fu_cv_lib_c_bcopy=no)])
  if test $fu_cv_lib_c_bcopy = yes; then
    AC_DEFINE(HAVE_BCOPY_IN_STRINGS_H, 1,
      [Define if bcopy et al are defined in the <strings.h> header file])
  fi)

dnl check for library providing md5 checksumming
AC_SUBST(LIBMD5)
AC_SUBST(MD5C)
AC_DEFUN(CODA_CHECK_MD5,
  [if test "${OPENSSL}" = "yes" ; then
    AC_CHECK_HEADERS(openssl/md5.h)
    AC_SEARCH_LIBS(MD5_Init, crypto,
      [test "$ac_cv_search_MD5_Init" = "none required" || LIBMD5="$ac_cv_search_MD5_Init"
       LIBS="$ac_func_search_save_LIBS"])
  fi
  if test "$ac_cv_search_MD5_Init" = "no"; then
      MD5C="md5c.o"
  fi])

dnl check whether we have scandir
AC_DEFUN(CODA_CHECK_DIRENT, 
  AC_CACHE_CHECK(for d_namlen in dirent, fu_cv_lib_c_dirent_d_namlen,
    [AC_TRY_COMPILE([#include <dirent.h>],
       [ struct dirent d; int n; n = (int) d.d_namlen; ],
       fu_cv_lib_c_dirent_d_namlen=yes,
       fu_cv_lib_c_dirent_d_namlen=no)])
  if test $fu_cv_lib_c_dirent_d_namlen = yes; then
    AC_DEFINE(DIRENT_HAVE_D_NAMLEN, 1,
      [Define if you have d_namlen in struct dirent])
  fi
  AC_CACHE_CHECK(for d_reclen in dirent, fu_cv_lib_c_dirent_d_reclen,
    [AC_TRY_COMPILE([#include <dirent.h>],
       [ struct dirent d; int n; n = (int) d.d_reclen; ],
       fu_cv_lib_c_dirent_d_reclen=yes,
       fu_cv_lib_c_dirent_d_reclen=no)])
  if test $fu_cv_lib_c_dirent_d_reclen = yes; then
    AC_DEFINE(DIRENT_HAVE_D_RECLEN, 1,
      [Define if you have d_reclen in struct dirent])
  fi)

dnl ---------------------------------------------
dnl Accept user defined path leading to libraries and headers.

AC_DEFUN(CODA_OPTION_SUBSYS,
  [AC_ARG_WITH($1,
    [  --with-$1=<prefix>	Install prefix of $1],
    [ CFLAGS="${CFLAGS} -I`(cd ${withval} ; pwd)`/include"
      CXXFLAGS="${CXXFLAGS} -I`(cd ${withval} ; pwd)`/include"
      LDFLAGS="${LDFLAGS} -L`(cd ${withval} ; pwd)`/libs" ])
    ])

AC_DEFUN(CODA_OPTION_OPENSSL,
  AC_ARG_WITH(openssl,
    [  --without-openssl     Do not link against the openssl library],
    [OPENSSL=${withval}], [OPENSSL=yes]))

dnl ---------------------------------------------
dnl Search for an installed library in:
dnl	 /usr/lib /usr/local/lib /usr/pkg/lib ${prefix}/lib

AC_DEFUN(CODA_FIND_LIB,
 [AC_CACHE_CHECK(location of lib$1, coda_cv_path_$1,
  [saved_CFLAGS="${CFLAGS}" ; saved_LDFLAGS="${LDFLAGS}" ; saved_LIBS="${LIBS}"
   coda_cv_path_$1=none ; LIBS="-l$1 $4"
   for path in default /usr /usr/local /usr/pkg ${prefix} ; do
     if test ${path} != default ; then
       CFLAGS="-I${path}/include ${CFLAGS}"
       LDFLAGS="-L${path}/lib ${LDFLAGS}"
     fi
     AC_TRY_LINK([$2], [$3], [coda_cv_path_$1=${path} ; break])
     CFLAGS="${saved_CFLAGS}" ; LDFLAGS="${saved_LDFLAGS}"
   done
   LIBS="${saved_LIBS}"
  ])
  case ${coda_cv_path_$1} in
    none) AC_MSG_ERROR("Cannot determine the location of lib$1")
          ;;
    default)
	  ;;
    *)    CFLAGS="-I${coda_cv_path_$1}/include ${CFLAGS}"
          CXXFLAGS="-I${coda_cv_path_$1}/include ${CXXFLAGS}"
          LDFLAGS="${LDFLAGS} -L${coda_cv_path_$1}/lib" 
	  if test x$RFLAG != x ; then
            LDFLAGS="${LDFLAGS} -R${coda_cv_path_$1}/lib "
	  fi
          ;;
  esac])

AC_DEFUN(CODA_RFLAG,
  [ dnl put in standard -R flag if needed
    if test x${RFLAG} != x ; then
      if test x${libdir} != xNONE ; then
        LDFLAGS="${LDFLAGS} -R${libdir}"
      elif test x${prefix} != xNONE ; then
        LDFLAGS="${LDFLAGS} -R${prefix}/lib"
      else 
        LDFLAGS="${LDFLAGS} -R${ac_default_prefix}/lib"
      fi
    fi])

