
trace_%:
	stty -F /dev/tty$* 115200 raw -echo; socat -v READLINE /dev/tty$*,b115200,rawer > /dev/null 2> $@.out;

# record.out:
# 	socat -x READLINE /dev/ttyUSB0,b9600,rawer,parenb=0 2>record_stderr;

sendstr.%:
	 echo -ne "$*" > /dev/ttyUSB0;
