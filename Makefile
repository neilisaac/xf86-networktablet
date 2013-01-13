
networktablet_drv.la : networktablet.lo
	libtool --mode=link gcc -module -o networktablet_drv.la -rpath /usr/lib/xorg/modules/input networktablet.lo

networktablet.lo : networktablet.c
	libtool --mode=compile gcc -c `pkg-config --cflags xorg-server` networktablet.c


clean :
	rm -r  .libs  *.la *.lo *.o

install :
	libtool --mode=install install -c networktablet_drv.la '/usr/lib/xorg/modules/input'
