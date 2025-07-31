set x 0
while {< $x 5} {
  set x [+ $x 1]
  puts $x
  continue
  puts bad
}

