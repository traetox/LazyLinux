# LazyLinux
A simple sleep daemon that watches idle time and sends the machine into S3 sleep, unless SSH is active.

LazyLinux will background and will monitor the idle time and throw the machine into sleep when the idle time
expires and all the SSH connections are closed.
