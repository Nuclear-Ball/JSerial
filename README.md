# JSerial
Simple C++ serializer

# How to use:
# Initializing:
1) Create template using JSerial::JsTemplate
2) Add types using .add_basic_type<(pod datatуpe)>(), .add_array<(pod datatуpe of the element)>() or .add_template(), yes, you can nest templates, and even make template arrays
3) Create serializer object with JSerial::JSerial
4) Do .main_template = <your mаin template>; .init_write() or .init_read();
# Writing
1) Write to your serializer object data with .write_basic_data<>(), .write_basic_array<>(). Note that you MUST write types in the order, which they are present in the template. Also you should know that these functions does not support strings and you should use .write_string() and .write_string_array() with strings. To initialize template array write you should use .prepare_template_array_write(). The write in regular templates is being initialized automatically.
2) Done! Then save output .data string somewhere
# Reading
