#include <iostream>
#include <nanodbc/nanodbc.h>
#include <exception>
#include "Service.h"

using namespace std;

int main()
{
    try {
        Service service(NANODBC_TEXT("Driver={ODBC Driver 17 for SQL Server};Server=.\\SQLEXPRESS;Database=Trypo;Trusted_Connection=yes;"));
        cout << "Server running. Press Enter to stop." << endl;
        cin.get();
    }
    catch (const std::exception& e) {
        cerr << "Eroare: " << e.what() << endl;
    }

    return 0;
}