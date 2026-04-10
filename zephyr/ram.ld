/* Reserve retained memlog storage inside Zephyr's existing .noinit output section. */
. = ALIGN(4);
__memlog_base__ = .;
. += 4K;
__memlog_end__ = .;
