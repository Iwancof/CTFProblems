#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<iostream>
#include<vector>

class BaseMemo {
	public:
		long memo[10];

		BaseMemo() {
			for(int i = 0;i < 10;i++) {
				memo[i] = -1; // unuse!
			}
		}
		void debug() {
			int index = get_unuse_index();
			if(memo[index] != -1) {
				printf("expected -1, got %ld\n", memo[index]);
				return;
			}
			puts("test passed");
		}

		virtual void show() = 0;
		virtual void edit() {
			std::cout << "Enter memo index: ";
			int index;
			std::cin >> index;
			if(index < 0 || 10 <= index) {
				puts("Out of range!");
				return;
			}
			long value;
			std::cout << "Enter value: ";
			std::cin >> value;

			memo[index] = value;
		}
		virtual void add() {
			std::cout << "What number do you want to add? :";
			long value;
			std::cin >> value;
			add_internal(value);
		}
		virtual ~BaseMemo() {
			for(int i = 0;i < 10;i++) {
				memo[i] = 0;
				// Clear value is secure!
			}
		}
	protected:
		int get_unuse_index() {
			for(int i = 0;i < 10;i++) {
				if(memo[i] == -1) {
					return i;
				}
			}
			return -1; // not found!
		}
		void add_internal(long value) {
			int index = get_unuse_index();
			memo[index] = value;
		}
};

class SumMemo: public BaseMemo {
	public:
		virtual void show() {
			long sum = 0;
			for(int i = 0;i < 10;i++) {
				if(memo[i] == -1) {
					break;
				}
				sum += memo[i];
			}
			printf("sum: %hx\n", sum);
			// I can't fix this warning....
memo[0] = sum; // store the value
		}
};

class ProdMemo: public BaseMemo {
	public:
		virtual void show() {
			long product = 0; // TODO: fix it!!!
			for(int i = 0;i < 10;i++) {
				if(memo[i] == -1) {
					break;
				}
				product *= memo[i];
			}
			printf("product: %hx\n", product);
			// Hmmm..... I can't fix this warning too....

			memo[0] = product;
		}
};
#define MEMO_LENGTH (0x8 * 3)
class StringMemo: public BaseMemo {
	public:
		virtual void add() {
			char* buffer = (char*)malloc(MEMO_LENGTH + 1);
			BaseMemo::add_internal((long)buffer); // this program is 64bit
			std::cout << "Enter contents: ";
			read(0, buffer, MEMO_LENGTH);
			buffer[MEMO_LENGTH] = 0;
		}
		virtual void edit() {
			std::cout << "StringMemo can not edit\n";
		}
		virtual void show() {
			for(int i = 0;i < 10;i++) {
				if(memo[i] == -1) {
					return;
				}
				char* s = (char*)memo[i];
				puts(s);
			}
		}
		virtual ~StringMemo() {
			for(int i = 0;i < 10;i++) {
				if(memo[i] == -1) {
					return;
				}
				free((void*)memo[i]);
				memo[i] = 0;
			}
		}
};

std::vector<BaseMemo*> pages = std::vector<BaseMemo*>();

void add_page() {
	printf("what kind page do you want to add?\n");
	printf(
			"1: Sum\n"
			"2: Product\n"
			"3: String\n"
			"kind:"
			);
	int opt;
	std::cin >> opt;

	switch(opt) {
		case 1: {
			pages.push_back(new SumMemo());
			break;
		}
		case 2: {
			pages.push_back(new ProdMemo());
			break;
		}
		case 3: {
			pages.push_back(new StringMemo());
			break;
		}
		default: {
			std::cout << "Hmmm...." << "\n";
		}
	}
}

int menu() {
	puts("what do you want to do?");
	printf(
			"0: exit\n"
			"1: add page\n"
			"2: add memo\n"
			"3: edit memo\n"
			"4: show all pages\n"
			"5: debug page\n"
			"6: delete page\n"
			"do:"
			);
	int ret;
	std::cin >> ret;
	
	return ret;
}


int main() {
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	int opt;
	while((opt = menu())) {
		switch(opt) {
			case 1:  {
				add_page();
				break;
			}
			case 2: {
				int index;
				std::cout << "Enter page index: ";
				std::cin >> index;
				pages.at(index)->add();
				break;
			}
			case 3: {
				int index;
				std::cout << "Enter page index: ";
				std::cin >> index;
				pages.at(index)->edit();
				break;
			}
			case 4: {
				for(auto it = pages.begin(); it != pages.end(); it++) {
					(*it)->show();
				}
				break;
			}
			case 5: {
				int index;
				std::cout << "Enter page index: ";
				std::cin >> index;
				pages.at(index)->debug();
				break;
			}
			case 6: {
				int index;
				std::cout << "Enter page index: ";
				std::cin >> index;
				delete pages.at(index);
				pages.erase(pages.begin() + index);
			}
		}
	} 
}

