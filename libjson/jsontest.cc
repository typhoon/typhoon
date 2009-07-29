#include "json.h"

int main (int argc, char* argv[]) {
    std::cout << "---test start---\n";

    try {
            JsonValue* json_value;
            std::string json_imp = "";
            std::string json_exp = "";

            std::cout << "testing simple values\n";
 
            json_imp = "true";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;

            json_imp = "false";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;

            json_imp = "null";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;

            json_imp = "1.755";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;

            json_imp = "123";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;

            json_imp = "\"sample\"";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;

            json_imp = "[]";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;

            json_imp = "{\"url\":\"teststring\"}";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            std::string s = json_value->get_value_by_tag("url")->get_string_value();
            std::cout << s << "\n";
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;

            std::cout << "more complicated\n";            

            json_imp = "{\"data1\":true, \"data2\":\"string\", \"data3\":[\"array1\", \"array2\"], \"data4\":0.0012 }";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;


            json_imp = "[{\"nest1\":{\"nest2\":{\"nest3\":{\"nest4\":{}}, \"value\":\"match nest3\" }, \"value\":\"match nest2\"}}, \"outer value\"]";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;

            std::cout << "data construction test\n";             

            json_value = new JsonValue(json_object);

            JsonValue* array_val = new JsonValue(json_array);
            array_val->add_to_array(new JsonValue(100));
            array_val->add_to_array(new JsonValue(200));
            array_val->add_to_array(new JsonValue(300));
            array_val->add_to_array(new JsonValue(400));
            std::cout << JsonExport::json_export(array_val) << "\n";

            JsonValue* obj_val = new JsonValue(json_object);
            obj_val->add_to_object("A", new JsonValue(-0.123));
            obj_val->add_to_object("B", new JsonValue(0.6655555));
            obj_val->add_to_object("C", new JsonValue(-0.1));
            std::cout << JsonExport::json_export(obj_val) << "\n";


            std::cout << JsonExport::json_export(json_value) << "\n";
            json_value->add_to_object("integer", new JsonValue(100));
            std::cout << JsonExport::json_export(json_value) << "\n";
            json_value->add_to_object("null", new JsonValue());
            std::cout << JsonExport::json_export(json_value) << "\n";
            json_value->add_to_object("object", obj_val );
            std::cout << JsonExport::json_export(json_value) << "\n";
            json_value->add_to_object("array", array_val ); 
            std::cout << JsonExport::json_export(json_value) << "\n";

            std::cout << "data access test\n";
            std::cout << JsonExport::json_export(json_value->get_value_by_tag("array")) << "\n";
            std::cout << JsonExport::json_export(json_value->get_value_by_tag("array")->get_value_by_index(2)) << "\n";
            std::cout << JsonExport::json_export(json_value->get_value_by_tag("null"))  << "\n";
            delete json_value;

            std::cout << "multibyte and escape test\n";
            json_imp = "[\"関東\", \"\\/\\u30a2\\u30a4\\u30a6\\u30a8\\u30aa\\/\\n\", \"\\u8a66\\t\\u5199\\n\"]";
            std::cout << "import: " << json_imp << "\n";
            json_value = JsonImport::json_import(json_imp);
            std::cout << "first native string: " << json_value->get_value_by_index(0)->get_string_value() << "\n";
            std::cout << "second native string: " << json_value->get_value_by_index(1)->get_string_value()  << "\n";
            std::cout << "second native string: " << json_value->get_value_by_index(2)->get_string_value()  << "\n";
            json_exp = JsonExport::json_export(json_value);
            std::cout << "export: " << json_exp << "\n";
            delete json_value;
            
    } catch (...) {
        std::cout << "test error occured!!\n";
        exit(1);
    }

    std::cout << "----test completed!!!---\n";

    return 0;
}
