Minilink Linker
===============
The Minilink linker produces much smaler modules, then the ELF linker.
Although the actual linker is bigger, then the ELF Linker, it needs less flash memory, because the symbol table can be saved on external flash.

Example
-------
The example _should_ run out of the box, by opening the csc on Cooja.
It loads an mlk module and links it against the current lernel.
The symbol table and module are loaded to the external flash using the test script.
On real nodes you must come up with a solution for doing this yourself.

Tools
-----
The tools are responsible for creating the symbol table and the module.
They need the LibBFD. 
You might need to adjust `BFDLIBDIR` in `tools/Makefile`
I the provided version does not work with your system you might need to compile it yourself.

Building your own LibBFD
------------------------
```sh
git clone git://git.code.sf.net/p/mspgcc/binutils msp-binutils
cd msp-binutils
./configure --target=msp430
make
```
Update files in libbfd-* folder


License
=======
BSD http://www.opensource.org/licenses/bsd-license.php


Contact
=======
Moritz "Morty" Strübe <morty@cs.fau.de>
