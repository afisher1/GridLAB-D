#!/bin/bash

export DOCS=i:/html/documents

cp utilities/troubleshooting.css $DOCS
./utilities/troubleshooting.awk */*.{c,cpp} > $DOCS/troubleshooting.html
