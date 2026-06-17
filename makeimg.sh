#!/bin/bash
# Linux equivalent of makeimg.bat
# Builds the Corman Lisp image file by loading and compiling all .lisp sources.

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

rm -f "$SCRIPT_DIR/CormanLisp.img"
"$SCRIPT_DIR/clconsole" -execute "$SCRIPT_DIR/sys/compile-sys.lisp" -image ""

echo "Image built: CormanLisp.img"
