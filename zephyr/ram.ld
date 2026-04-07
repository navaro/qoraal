
/* --- Uninitialized RAM for memlogger (~4 KiB here) --- */
SECTION_PROLOGUE(.memlog, (NOLOAD),)
{
    . = ALIGN(4);
    __memlog_base__ = .;
    . += 4K;
    __memlog_end__ = .;
} GROUP_NOLOAD_LINK_IN(RAMABLE_REGION, RAMABLE_REGION)