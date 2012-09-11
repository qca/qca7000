#!/bin/sh

STARTDIR="${PWD}"
DRIVERS="qca-spi-0.1.8 qca-uart-0.0.5 qca-bootstrap-0.0.4 qca-pl16cfg-0.0.1"
OUTPUT_DIR="/home/nhoughton/mx28/ltib/qca_packages"

for driver in ${DRIVERS}; do
	rm -rf "${driver}" &&
	basename="$(echo "${driver}" | sed 's/^\(qca-.*\)-[0-9]\.[0-9]\.[0-9]$/\1/')" &&
	cp -r "${basename}" "${driver}" &&
	cd "${STARTDIR}" &&
	tar -czvf "${OUTPUT_DIR}/${driver}.tar.gz" "${driver}" &&
	cd "${STARTDIR}" &&
	rm -rf "${driver}"
	if [ $? -ne 0 ]; then
		echo "Failed on driver ${driver}."
		exit 1
	fi
done
