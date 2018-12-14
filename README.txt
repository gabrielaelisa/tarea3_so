Este ejemplo es una adaptacion del tutorial incluido
(archivo "device drivers tutorial.pdf") y bajado de:
http://www.freesoftwaremagazine.com/articles/drivers_linux

---

Guia rapida:

Lo siguiente se debe realizar parados en
el directorio en donde se encuentra este README.txt

+ Compilacion (puede ser en modo usuario):
$ make
...
$ ls
... syncwrite.ko ...

+ Instalacion (en modo root)

# mknod /dev/syncwrite0 c 65 0
# mknod /dev/syncwrite1 c 65 1
# chmod a+rw /dev/syncwrite0 /dev/syncwrite1
# insmod syncwrite.ko
# dmesg | tail
...
[...........] Inserting syncwrite module
#

+ Testing (en modo usuario preferentemente)

Ud. necesitara crear multiples shells independientes.  Luego
siga las instrucciones del enunciado de la tarea 3.

+ Desinstalar el modulo

# rmmod syncwrite.ko
#
