#!/bin/sh

rm -f *.md5

for file in *.tar.gz *.patch; do
	md5sum "${file}" > "${file}.md5"
	if [ $? -ne 0 ]; then
		echo "Error: could not generate md5sum for ${file}."
		exit 1
	fi
done
