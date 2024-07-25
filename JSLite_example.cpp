#include "JSerialLite.hpp"
#include <iostream>

int main() {
    //
    JSerial::Write a(JSerial::CalculateTemplateSize<size_t>( {JSerial::Type<int>(),
                                                              JSerial::DynamicType(),
                                                              JSerial::DynamicType()}));

    a.WriteStaticData<int>(16)
    .WriteDynamicData("string test")    
    .WriteStringArray({"arr test", "1", "234", ""});                              
    const std::string res = a.Result();  

    JSerial::Read b(res);
    std::cout << b.GetStaticData<int>() << " " << b.GetDynamicData();

    const std::vector<std::string> vecstr = b.GetStringArray();
    for(const std::string& i : vecstr)
            std::cout << i << "\n";

    for(int i = 0; i < res.size(); i++){                                                                                                                             
            std::cout << i << " \42" << res[i] << "\42 [" << static_cast<int>(res[i]) << "]\n";                                                                      
    }                                                                                                                                                                
                                                                                                                                                                     
    std::cout << "end";                                                                                                                                              
    return 0;                                                                                                                                                            
}      
