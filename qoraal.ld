



/* --- Uninitialized RAM for memlogger (~10 KiB) --- */
SECTION_DATA_PROLOGUE(.memlog, (NOLOAD),)
{
    . = ALIGN(4);
    __memlog_base__ = .;
    . += 10K;                 /* change to 5K for ~5 KiB */
    __memlog_end__ = .;
} GROUP_LINK_IN(RAMABLE_REGION)

/* --- QShell command table in ROM, */
.qshell : ALIGN(32)
{
    . = ALIGN(32);
    __qshell_cmds_base__ = .;
    KEEP(*(SORT(.qshell.cmds.*)))
    __qshell_cmds_end__  = .;
} GROUP_LINK_IN(ROMABLE_REGION)