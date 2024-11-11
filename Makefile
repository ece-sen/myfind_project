all: myfind

myfind: myfind.cpp
	# Note: Ensure the line below starts with a tab, not spaces.
	g++ -std=c++17 -Wall -Werror -o myfind myfind.cpp -lpthread

clean:
	# Note: Ensure this line also starts with a tab.
	rm -f myfind
