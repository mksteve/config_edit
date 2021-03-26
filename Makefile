config_edit_objs=main.o
config_edit_libs=
CXXFLAGS=-g


config_edit : $(config_edit_objs)
	g++ -g -o config_edit $(config_edit_objs) $(config_edit_libs)

main.o : json_lite.h
