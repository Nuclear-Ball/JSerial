# JSerial
A simple C++ serializer

# Usage:
View "JSLite_example.cpp" to understand how this works. Also note that "jserial.h" is no longer supported(even if it was published recently), it was last updated more than 6 months ago, its pretty slow, and i generaly dont recommend using it, unless it's easier for you to see type names when reading data and have error messages. Im not even sure if data produced by both serializers is even compatible...

# Known issues: 
The serializer doesn't save any type information, it assumes that both sender and reciever have the same data template. This makes it much more faster and memory efficient, howewer if sender and reciever have even slightly different templates(for example if using different versions of the same software), it might have undefined behaviour.
