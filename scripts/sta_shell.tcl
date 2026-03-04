if {$argc < 3} {
    puts "Usage: tclsh sta_shell.tcl <liberty> <netlist> <constraints> ?extra args...?"
    exit 1
}

set exe "./sta"
if {$tcl_platform(platform) eq "windows"} {
    set exe "./sta.exe"
}

set cmd [list $exe --liberty [lindex $argv 0] --netlist [lindex $argv 1] --constraints [lindex $argv 2]]
for {set i 3} {$i < $argc} {incr i} {
    lappend cmd [lindex $argv $i]
}

puts "Running: $cmd"
set status [catch {eval exec $cmd} output]
puts $output
exit $status
