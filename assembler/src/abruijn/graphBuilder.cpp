#include <iostream>
#include <algorithm>
#include "graph.hpp"
#include "hash.hpp"
#include "graphBuilder.hpp"
#include "parameters.hpp"
#include "../parser.hpp"
#include <ext/hash_map>

using namespace std;
using namespace __gnu_cxx;

typedef hash_map< Sequence, CVertex, HashSym<Sequence> > seq2ver;
seq2ver kmers_map;
CGraph graph;

void processRead(Seq<MPSIZE> r) {
	// Processing in O(length * k).
	// Can be done in O(length), provided the hash is polynomial.
	unsigned int h1 = -1;
	int pos1 = -1;
	unsigned int h2 = -1;
	int pos2 = -1;
	for (int i = 0; i + K <= MPSIZE; i++) {
		HashSym<Sequence> hh;
		//Sequence* kmer = &);
		unsigned int h = hh(r.substring(i, i + K));
		if (h < h1) {
			h2 = h1;
			pos2 = pos1;
			h1 = h;
			pos1 = i;
		} else if (h < h2) {
			h2 = h;
			pos2 = i;
		}
	}
	if (pos1 > pos2) {
		int t = pos1; pos1 = pos2; pos2 = t;
		t = h1; h1 = h2; h2 = t;
	}
	Sequence k1 = r.substring(pos1, pos1 + K);
	if (kmers_map.count(k1)) {
		CVertex v(&k1);
	} else {

	}
	Sequence k2 = r.substring(pos2, pos2 + K);
	CVertex v1 = kmers_map[k1];
	CVertex v2 = kmers_map[r.substring(pos2, pos2 + K)];
	v1.hits_++;
	graph.AddVertex(v1);
	graph.AddVertex(v2);
	CEdge e(&v2, &r, pos1, pos2);
	v1.AddEdge(e);
}

CGraph GraphBuilder::build() {
	FASTQParser<MPSIZE>* fqp = new FASTQParser<MPSIZE>();
	fqp->open(filenames.first, filenames.second);
	while (!fqp->eof()) {
		MatePair<MPSIZE> r = fqp->read(); // is it copy? :)
		if (r.id == -1) { // don't have 'N' in reads
			continue;
		}
		processRead(r.seq1);
		processRead(r.seq2);
		if (r.id == 100000) {
			break;
		}
		//cout <<  mp.id << endl << mp.seq1.str() << endl <<  mp.seq2.str() << endl;
	}
	debug(kmers_map.size());
	seq2ver::iterator p;
	for (p = kmers_map.begin(); p != kmers_map.end(); ++p) {
		debug((p->first).Str());
		debug((p->second).hits_);
	}
	return graph;
}
