# config_edit
A piece of code which allows config.txt to be edited by additions, removals and comments.
The code is [filter] aware, so tries to add lines in config.txt to the correct location of the file.

config.txt editor

Usage : ./config-edit

  -a, --add string        Add string in a section which
                          matches the final filter.
                          
      --all               Set the filter to all.
      
  -c, --comment string    Comment the line 'string' In
                          the final filter
                          
      --config cfg_json   Use alternative json file for filters
      
  -e, --edid edid=value   Set the filter to include EDID
  
  -f, --file config_name  Act on config_name instead of 
                          config.txt
                          
  -g, --gpio gpioX=[0|1]  Set gpio filter
  
      --hdmi  HDMI:[0|1]  Filter for each hdmi [pi4]
      
      --keepbackup        Don't remove .bak file
      
  -p, --platform plt      Set the platform to {pi0, pi0w,
                          pi1, pi2, pi3, pi3+,pi4}
                          
  --print                 Display current config.txt
  
  --remove string        Remove the string from the filter


Default configuration is :-
 {
 
  "platform"  : [ "pi0", "pi0w" , "pi1", "pi2", "pi3", "pi3+", "pi4" ],
  
  "super" :     [ "all", "none" ],
  
  "edid" :      [ "edid" ],
  
  "cpuserial" : [ "0x%x" ], 
  
  "hdmi" :      [ "HDMI:0", "HDMI:1" ],
  
  "gpio" :      [ "gpio%d" ] 
  
  }

