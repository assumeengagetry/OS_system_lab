#!/bin/bash


if [ -z "$1" ]; then
    echo "usage: $0 <file_name>"
    exit 1
fi

file="$1"


if [ -e "$file" ]; then
    size=$(stat -c %s "$file")
    echo "file_name: $file, file_size: $size "
else 
    echo "File not found"
fi
