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

    void removeBook(){
        int id;
        cout<<"Enter Book ID to remove: ";
        cin>>id;

        for(auto it = books.begin(); it != books.end(); ++it){
            if(it->id == id){
                if(it->isIssued){
                    cout<<"Cannot remove book! It is currently issued.\n";
                    return;
                }
                books.erase(it);
                cout<<"Book removed successfully!\n";
                return;
            }
        }
        cout<<"Book Not Found.\n";
    }

    void updateBook(){
        int id;
        cout<<"Enter Book ID to update: ";
        cin>>id;
        cin.ignore();

        for(Book& book : books){
            if(book.id == id){
                string newTitle, newAuthor;

                cout<<"Current Title: "<<book.title<<"\n";
                cout<<"Enter New Title (press Enter to keep unchanged): ";
                getline(cin, newTitle);
                if(!newTitle.empty()){
                    book.title = newTitle;
                }

                cout<<"Current Author: "<<book.author<<"\n";
                cout<<"Enter New Author (press Enter to keep unchanged): ";
                getline(cin, newAuthor);
                if(!newAuthor.empty()){
                    book.author = newAuthor;
                }

                cout<<"Book details updated successfully!\n";
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
        cout<<"6.Remove Book\n";
        cout<<"7.Update Book\n";
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
            case 6:
                library.removeBook();
                break;
            case 7:
                library.updateBook();
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