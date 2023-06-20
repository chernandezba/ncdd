#!/bin/sh
#
# Controlador de dispositius en xarxa
#
# Treball de fi de carrera
# Alumne: Cesar Hernandez Baño
# Carrera: Enginyeria Tecnica Informatica de Sistemes
#
# Fitxer crear_dispositius.sh
# Creació de les entrades de dispositius
#


cd /dev
mkdir ncdd
cd cdx

mknod ncdd c 254 250

mkdir localhost
cd localhost

mknod dsp c 254 0
mknod lp0 c 254 1
mknod fb0 c 254 2
mknod zero c 254 3

echo "Dispositius creats."
