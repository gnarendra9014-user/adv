#include <iostream>
#include <vector>
#include <string>
using namespace std;

class Book{
public: 
    int id;
    string title;
    string author;
    bool isIssued;
    

    Book(int id , string title, string author){
        this->id=id;
        this->title=title;
        this->author=author;
        this->isIssued=false;
    }
};

class Library{
public:
    void addBook(){
        int id;
        string title, author;

        cout<<"Enter Book ID : ";
        cin>> id;
        cin.ignore();

        cout<< "Enter a Book title :";
        getline(cin,title);

        cout<< "Enter Author Name:";
        getline(cin,author);

        for(const Book& book:books){
            if(book.id==id){
                cout<<"Book already exists!\n";
                return;
            }
        }

        books.emplace_back(id,title,author);
        cout<<"Book Added Successfully!\n";

    }

    void displayBook(){
        if(books.empty()){
            cout<<"No books available.\n";
            return;
        }
        for(const Book& book:books){
            cout<< "\nBookID:"<<book.id
                <<"\nTitle :" <<book.title
                <<"\nAuthor : "<<book.author
                <<"\nStatus : "
                <<(book.isIssued ? "Issued" : "Available")
                << "\n";
        }
    }

    void searchBook(){
        int id;
        cout<<"Enter BookId to Search:";
        cin>>id;

        for(const Book& book:books){
            if(book.id==id){
                cout<<"Book Found!\n"
                    <<"Title :"<<book.title<<"\n"
                    <<"Author :"<<book.author<<"\n"
                    <<"Status :"
                    <<(book.isIssued ? "Issued ": "Available")
                    <<"\n";
                return;
            }
        }
        cout<<"Book Not Found.\n";
    }

    void issueBook(){
        int id;
        cout<<"Enter Book ID to issue";
        cin>>id;
        for(Book& book:books){
            if(book.id==id){
                if(book.isIssued){
                    cout<<"Book is already Issued.\n";
                }else{
                    book.isIssued=true;
                    cout<<"Book issued Successfully!\n";
                }
                return;
            }
        }
        cout<<"Book Not Found.\n";
    }

    void returnBook(){
        int id;
        cout<<"Enter BookId to return";
        cin>>id;

        for(Book& book:books){
            if(book.id==id){
                if(!book.isIssued){
                    cout<<"Book was not issued.\n";
                }else{
                    book.isIssued=false;
                    cout<<"Book returned Successfully\n";
                }
                return;
            }
        }
        cout<<"Book Not Found.\n";
    }

private:
    vector<Book> books;
};

int main() {
    Library library;
    int choice;

    do{
        cout<<"\n=====LIBRARY MANAGEMENT SYSTEM =====\n";
        cout<<"1.AddBook\n";
        cout<<"2.Display AllBooks\n";
        cout<<"3.Search Book\n";
        cout<<"4.Issue Book\n";
        cout<<"5.Return Book\n";
        cout<<"0.Exit\n";
        cout<<"Enter Your Choice: ";
        cin>>choice;

        switch(choice){
            case 1:
                library.addBook();
                break;
            case 2:
                library.displayBook();
                break;
            case 3:
                library.searchBook();
                break;
            case 4:
                library.issueBook();
                break;
            case 5:
                library.returnBook();
                break;
            case 0:
                cout<<"Exiting system....\n";
                break;
            default:
                cout<<"Invalid choice. Try Again.\n";
        }
    }while(choice!=0);

    return 0;
}