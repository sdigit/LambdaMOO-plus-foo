# Third-Party Licenses and Notices

This file documents the copyright and licensing layers present in this source
tree. Every `.c`/`.h` file in the tree carries at least one license notice;
there are no undocumented/"all rights reserved with no grant" files.

## 1. Primary license — BSD-2-Clause (Sean Davis)

Applies to essentially every `.c` and `.h` file in the tree (top-most header
block), e.g.:

```
Copyright (c) 2002-2026
              Sean Davis <dive@endersgame.net>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES ... [full BSD-2-Clause disclaimer]
```

This is standard BSD-2-Clause: no advertising clause, no copyleft.

## 2. Nested upstream license — Xerox Corporation (1992/1995/1996)

Nearly every file with the Sean Davis header above also nests, immediately
beneath it, the original LambdaMOO license from Xerox PARC (Pavel Curtis).
Some files additionally attribute portions to Stephen White ("ghond").

```
Copyright (c) 1992, 1995, 1996 Xerox Corporation. All rights reserved.
Portions of this code were written by Stephen White, aka ghond.
Use and copying of this software and preparation of derivative works based
upon this software are permitted. Any distribution of this software or
derivative works must comply with all applicable United States export
control laws. This software is made available AS IS, and Xerox Corporation
makes no warranty about the software, its performance or its conformity to
any specification. Any person obtaining a copy of this software is requested
to send their name and post office or electronic mail address to:
  Pavel Curtis, Xerox PARC, 3333 Coyote Hill Rd., Palo Alto, CA 94304
  Pavel@Xerox.Com
```

Notes:
- This is a permissive grant, but **not** a standard OSI license text. It
  imposes an explicit US export-control compliance condition on redistribution.
- The request to email Xerox/Pavel Curtis is a courtesy request, not a
  binding condition of the grant.
- `md5.c` / `include/md5.h` carry a narrower version of this block, dated
  **1996 only** (not 1992/1995/1996), suggesting a separate/later Xerox
  contribution specific to the MD5 implementation.

## 3. `regexpr.c` — Tatu Ylonen (1991), permissive

`regexpr.c` carries a third, distinct layer beneath the Sean Davis / Xerox
headers:

```
regexpr.c
Author: Tatu Ylonen <ylo@ngs.fi>
Copyright (c) 1991 Tatu Ylonen, Espoo, Finland

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies. This
software is provided "as is" without express or implied warranty.

Modified from the Python-1.3 distribution for use in LambdaMOO
by Pavel Curtis, 27 October 1995.

This code draws many ideas from the regular expression packages by
Henry Spencer of the University of Toronto and Richard Stallman of the
Free Software Foundation. Emacs-specific code and syntax table code is
almost directly borrowed from GNU regexp.
```

This is a genuinely separate permissive grant (MIT/X11-style), not merely a
copyright-holder substitution. Retain this block intact in `regexpr.c`.

## 4. `keywords.c` — gperf-generated, license carried via `#line`

`keywords.c` is generated output (`gperf -aCIptT -k'1,3,$' keywords.gperf`).
The Sean Davis / Xerox header from the source `keywords.gperf` template is
carried into the generated file via `#line` directives, so the generated
file remains covered by items 1 and 2 above. No separate gperf license
applies to the *output* (gperf's own license only affects the gperf tool).

## 5. `include/y.tab.h` — GPLv3 (Bison), with the Bison parser-skeleton exception

This is the outlier. It is Bison-generated output and carries:

```
Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation, Inc.
Licensed under GNU GPLv3 or later.
```

immediately followed by Bison's standard exception clause:

```
As a special exception, you may create a larger work that contains
part or all of the Bison parser skeleton and distribute that work
under terms of your choice, so long as that work isn't itself a
parser generator using the skeleton or a modified version thereof
as a parser skeleton. Alternatively, if you modify or redistribute
the parser skeleton itself, you may (at your option) remove this
special exception, which will cause the skeleton and the resulting
Bison output files to be licensed under the GNU General Public
License without this special exception.
```

**Practical implication:** this exception is what allows `y.tab.h` to be
compiled into a non-GPL (BSD-licensed) server. It is conditional — if the
Bison skeleton itself is modified/regenerated by a different toolchain and
the exception text is dropped, the generated output reverts to plain GPLv3.
Any CMake/build-system change that regenerates this header should be checked
to confirm the exception clause is still present verbatim in the output.

## 6. RFC 6234 SHA implementation — IETF Trust (2011)

The SHA-256, SHA-384, and SHA-512 implementation imported from RFC 6234 is
covered by the copyright and license contained in:

- `include/ietf/sha.h`
- `include/ietf/sha-private.h`
- `sha256.c`
- `sha384-512.c`

The implementation is copyrighted by:

```
Copyright (c) 2011 IETF Trust and the persons identified as
authors of the code. All rights reserved.
```

The license is a standard BSD-3-Clause-style permissive license granting
redistribution and use in source and binary forms, with or without
modification, subject to the following conditions:

- Redistributions of source code must retain the copyright notice,
  conditions, and disclaimer.
- Redistributions in binary form must reproduce the copyright notice,
  conditions, and disclaimer in the accompanying documentation and/or
  other materials.
- Neither the name of the Internet Society, the IETF, the IETF Trust,
  nor the names of the contributors may be used to endorse or promote
  derived products without prior written permission.

The software is provided "AS IS", without express or implied warranties,
including merchantability or fitness for a particular purpose, and the
copyright holders and contributors disclaim liability for any damages.

This is a conventional permissive BSD-3-Clause-family license and is
compatible with the rest of this source tree.

## Summary table

| File(s) | License layer(s) | Notes |
|---|---|---|
| Nearly all `.c`/`.h` | BSD-2-Clause (Sean Davis, 2002–2007) over Xerox Corp. permissive (1992/1995/1996) | Xerox layer requires US export-control compliance on redistribution |
| `md5.c`, `include/md5.h` | Same stack, Xerox portion dated 1996 only | Narrower/separate Xerox claim |
| `regexpr.c` | + Tatu Ylonen 1991 permissive license | Distinct grant, ported from Python 1.3 by Pavel Curtis (1995) |
| `keywords.c` | BSD/Xerox stack carried via `#line` from `keywords.gperf` | gperf-generated; no separate gperf license on output |
| `include/y.tab.h` | GPLv3 + Bison special exception | Exception clause is required to avoid copyleft on the whole build |
| `sha256.c`, `sha384-512.c`, `include/ietf/sha.h`, `include/ietf/sha-private.h` | IETF Trust (2011) BSD-3-Clause-style	RFC 6234 reference implementation | includes standard non-endorsement clause |

No file in the tree is unlicensed.
