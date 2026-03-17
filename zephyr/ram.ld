
/* --- Uninitialized RAM for memlogger (~10 KiB) --- */
SECTION_DATA_PROLOGUE(.memlog, (NOLOAD),)
{
    . = ALIGN(4);
    __memlog_base__ = .;
    . += 4K;                 /* change to 5K for ~5 KiB */
    __memlog_end__ = .;

} GROUP_LINK_IN(RAMABLE_REGION)