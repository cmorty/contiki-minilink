SECTIONS
{
.text :
{
     . = ALIGN(2);
	*(.text .rodata.* .rodata)
     . = ALIGN(2);
}
}
