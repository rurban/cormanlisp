#!/bin/bash
# Linux equivalent of makeimg.bat
# Builds the Corman Lisp image file by loading and compiling all .lisp sources.

set -e  # exit on error (ignore for bootstrap)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CLCONSOLE="${CLCONSOLE:-$SCRIPT_DIR/build/clconsole}"

rm -f "$SCRIPT_DIR/CormanLisp.img"
"$CLCONSOLE" --batch -execute "$SCRIPT_DIR/Sys/compile-sys.lisp" --save-image "$SCRIPT_DIR/CormanLisp.img" -image ""
echo "Image built: CormanLisp.img"
