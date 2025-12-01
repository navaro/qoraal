





/* --- QShell command table in ROM, */
.qshell : ALIGN(32)
{
    . = ALIGN(32);
    __qshell_cmds_base__ = .;
    KEEP(*(SORT(.qshell.cmds.*)))
    __qshell_cmds_end__  = .;

} GROUP_LINK_IN(ROMABLE_REGION)

/* --- Engine sections in ROM, */
.engine : ALIGN(32)
{
    . = ALIGN((32));
    __engine_action_base__ = .;
    KEEP(*(SORT(.engine.engine_action.*)))
    . = ALIGN(4);
    __engine_action_end__ = .;
    __engine_event_base__ = .;
    KEEP(*(SORT(.engine.engine_event.*)))
    . = ALIGN(4);
    __engine_event_end__ = .;
    __engine_const_base__ = .;
    KEEP(*(SORT(.engine.engine_const.*)))
    . = ALIGN(4);
    __engine_const_end__ = .;
    __engine_cmd_base__ = .;
    KEEP(*(SORT(.engine.engine_cmd.*)))
    . = ALIGN(4);
     __engine_cmd_end__ = .;

}  GROUP_LINK_IN(ROMABLE_REGION)

/* --- Uninitialized RAM for memlogger (~10 KiB) --- */
SECTION_DATA_PROLOGUE(.memlog, (NOLOAD),)
{
    . = ALIGN(4);
    __memlog_base__ = .;
    . += 4K;                 /* change to 5K for ~5 KiB */
    __memlog_end__ = .;

} GROUP_LINK_IN(RAMABLE_REGION)