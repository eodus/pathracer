#include <cassert>
#include <iostream>
#include <list>
#include <vector>
#include "parser.hpp"

using namespace std;

pair<string,string> filenames = make_pair("./data/s_6_1.fastq.gz", "./data/s_6_2.fastq.gz");

int main(int argc, char *argv[]) {
  
	std::cerr << "Hello, I am assembler!" << std::endl;
  
	FASTQParser* fqp = new FASTQParser();
	fqp->open(filenames.first, filenames.second);
	while (!fqp->eof()) {
		MatePair mp = fqp->read();
		cout <<  mp.id << endl << mp.seq1 << endl << mp.seq2 << endl;
	}
	fqp->close();
	
	return 0;
}
