# All variables and this file are optional, if they are not present the PG and the
# makefiles will try to parse the correct values from the file system.
#
# Variables that specify exclusions can use % as a wildcard to specify that anything in
# that position will match. A partial path can also be specified to, for example, exclude
# a whole folder from the parsed paths from the file system
#
# Variables can be specified using = or +=
# = will clear the contents of that variable both specified from the file or the ones parsed
# from the file system
# += will add the values to the previous ones in the file or the ones parsed from the file
# system
#
# The PG can be used to detect errors in this file, just create a new project with this addon
# and the PG will write to the console the kind of error and in which line it is

meta:
	ADDON_NAME = ofxTwoscilloscope
	ADDON_DESCRIPTION = Vector shapes to XY oscilloscope audio and back again, ported from XYscope and Oscilloscope.
	ADDON_AUTHOR = @n1ckfg
	ADDON_TAGS = "addon" "audio" "graphics" "oscilloscope" "vector"
	ADDON_URL = http://github.com/n1ckfg/ofxTwoscilloscope

common:
	# dependencies with other addons, a list of them separated by spaces
	# or use += in several lines
	# ADDON_DEPENDENCIES =

	ADDON_INCLUDES = src

	# some addons need resources to be copied to the bin/data folder of the project
	# specify here any files that need to be copied, you can use wildcards like * and ?
	ADDON_DATA = data/hershey_fonts
