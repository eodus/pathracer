#include "constructHashTable.hpp"
#include "common.hpp"
#include "graphConstruction.hpp"
#include "../seq.hpp"
#include "../graphVisualizer.hpp"

int VertexCount;

using namespace paired_assembler;

void constructGraph() {
	//	readsToPairs(parsed_reads, parsed_k_l_mers);
	//	pairsToSequences(parsed_k_l_mers, parsed_k_sequence);
	cerr << "Read edges" << endl;
	edgesMap edges = sequencesToMap(parsed_k_sequence);
	cerr << "Go to graph" << endl;
	gvis::GraphScheme<int> g("Paired");
	cerr << "Start vertices" << endl;
	VertexCount = 0;
	createVertices(g, edges);
}

edgesMap sequencesToMap(string parsed_k_sequence) {
	FILE *inFile = fopen(parsed_k_sequence.c_str(), "r");

	vector<VertexPrototype *> prototypes;
	edgesMap res;
	prototypes.reserve(maxSeqLength);
	while (1) {
		char s[maxSeqLength];
		int size, scanf_res;
		ll kmer;
		scanf_res = fscanf(inFile, "%lld %d", &kmer, &size);
		//		cerr<<scanf_res;
		if ((scanf_res) != 2) {

			if (scanf_res == -1) {
				cerr << "sequencesToMap finished reading";
				break;
			} else {
				cerr << "sequencesToMap error in reading headers";
				continue;
			}
		}
		prototypes.clear();
		forn(i, size) {
			scanf_res = fscanf(inFile, "%s", s);
			if (!scanf_res) {
				cerr << "sequencesToMap error in reading sequences";
			}
			//			cerr <<s;
			Sequence *seq = new Sequence(s);
			//			cerr <<"seq = "<<seq->size(); //Magic
			//			cerr <<endl; //Magic
			VertexPrototype *v = new VertexPrototype();
			v->lower = seq;
			v->start = 0;
			v->finish = 0;
			v->used = 0;
			prototypes.pb(v);
		}
		res.insert(mp(kmer, prototypes));
	}
	return res;
}

void createVertices(gvis::GraphScheme<int> &g, edgesMap &edges) {
	int mass[21000], inD[21000], outD[21000];
	char Buffer[20];
	vertecesMap verts;
	cerr << "Start createVertices " << edges.size() << endl;
	forn(i,21000) {
		mass[i] = 0;
		inD[i] = 0;
		outD[i] = 0;
	}

	for (edgesMap::iterator iter = edges.begin(); iter != edges.end();) {
		int size = iter->second.size();
		ll kmer = iter->fi;
		cerr << "kmer " << kmer << " " << decompress(kmer, k - 1) << " Pairs "
				<< size << endl;
		forn(i, size) {
			if ((!(iter->se)[i]->used)) {
				int length = 1;
				(iter->se)[i]->used = 1;
				cerr << "seq " << (iter->se)[i]->lower->Str() << endl;
				//	cerr<<(iter->se)[i]->lower->Str()<<" "<<(iter->se)[i]->lower->size();
				ll finishKmer = kmer & (~((ll) 3 << (2 * (k - 1))));
				Sequence *finishSeq = new Sequence(
						(iter->se)[i]->lower->Str().c_str());
				ll startKmer = kmer >> 2;
				Sequence *startSeq = new Sequence(
						(iter->se)[i]->lower->Str().c_str());
				//cerr<<"expandDown "<<finishKmer<<" "<<finishSeq->Str()<<endl;
				length += expandDown(edges, verts, finishKmer, finishSeq);
				int toVert = storeVertex(verts, finishKmer, finishSeq);

				if (toVert == VertexCount - 1)
					g.addVertex(toVert, decompress(finishKmer, k - 1));
				length += expandUp(edges, verts, startKmer, startSeq);
				int fromVert = storeVertex(verts, startKmer, startSeq);
				if (fromVert == VertexCount - 1)
					g.addVertex(fromVert, decompress(startKmer, k - 1));
				cerr << "from " << fromVert << " to " << toVert << " length "
						<< length << endl;
				if (fromVert - toVert != 1)
					cerr << "GOOD" << endl;
				sprintf(Buffer, "%d", length);
				g.addEdge(fromVert, toVert, Buffer);
				mass[fromVert]++;
				outD[fromVert]++;
				mass[toVert]++;
				inD[toVert]++;

			}
		}
		edges.erase(iter++);
	}
		g.output();
	forn(i, VertexCount) {
		cerr << i << " " << mass[i] << " +" << inD[i] << " -" << outD[i]<< endl;
	}
}

int expandDown(edgesMap &edges, vertecesMap &verts, ll &finishKmer,
		Sequence* &finishSeq) {
	int length = 0;
	while (1) {
		vertecesMap::iterator iter = verts.find(finishKmer);
		if (iter != verts.end()) {
			int size = iter->second.size();
			//			cerr<<"size "<<size<< " find "<< finishSeq->Str()<< endl;
			forn(i, size) {
				//				cerr<<((iter->se)[i]->lower)->Str()<<endl;
				if (finishSeq->similar(*((iter->se)[i]->lower), k))
					return length;
			}
		}
		if (!CheckUnuqueWayUp(edges, finishKmer, finishSeq))
			return length;
		if (!GoUnuqueWayDown(edges, finishKmer, finishSeq))
			return length;
		else
			length++;
	}
}

int expandUp(edgesMap &edges, vertecesMap &verts, ll &startKmer,
		Sequence* &startSeq) {
	int length = 0;

	while (1) {
		vertecesMap::iterator iter = verts.find(startKmer);
		if (iter != verts.end()) {
			int size = iter->second.size();
			forn(i, size) {
				if (startSeq->similar(*((iter->se)[i]->lower), k))
					return length;
			}
		}
		if (!CheckUnuqueWayDown(edges, startKmer, startSeq))
			return length;
		if (!GoUnuqueWayUp(edges, startKmer, startSeq))
			return length;
		else
			length++;
	}
}

int CheckUnuqueWayUp(edgesMap &edges, ll finishKmer, Sequence *finishSeq) {
	int count = 0;
	for (int Nucl = 0; Nucl < 4; Nucl++) {
		ll tmpKmer = (ll) Nucl << (2 * (k - 1)) | finishKmer;
		edgesMap::iterator iter = edges.find(tmpKmer);
		if (iter != edges.end()) {
			int size = iter->second.size();
			forn(i, size) {
				if (finishSeq->similar(*((iter->se)[i]->lower), k)) {
					count++;
					if (count > 1)
						return 0;
				}
			}
		}
	}
	return count;
}

int GoUnuqueWayUp(edgesMap &edges, ll &finishKmer, Sequence* &finishSeq) {
	int count = 0;
	Sequence *PossibleSequence;
	ll PossibleKmer;
	int seqIndex;
	edgesMap::iterator PossibleIter;
	for (int Nucl = 0; Nucl < 4; Nucl++) {
		ll tmpKmer = (ll) Nucl << (2 * (k - 1)) | finishKmer;
		edgesMap::iterator iter = edges.find(tmpKmer);
		if (iter != edges.end()) {
			int size = iter->second.size();
			forn(i, size) {
				if (finishSeq->similar(*((iter->se)[i]->lower), k)) {
					count++;
					if (count > 1)
						return 0;
					PossibleKmer = tmpKmer;
					PossibleSequence = (iter->se)[i]->lower;
					seqIndex = i;
					PossibleIter = iter;

				}
			}
		}
	}
	if (count == 1) {
		finishKmer = PossibleKmer >> 2;
		finishSeq = PossibleSequence;
		(PossibleIter->se)[seqIndex]->used = 1;
		return 1;
	}
	return 0;
}

int GoUnuqueWayDown(edgesMap &edges, ll &finishKmer, Sequence* &finishSeq) {
	int count = 0;
	Sequence *PossibleSequence;
	ll PossibleKmer;
	int seqIndex;
	edgesMap::iterator PossibleIter;
	for (int Nucl = 0; Nucl < 4; Nucl++) {
		ll tmpKmer = (ll) Nucl | (finishKmer << (2));
		edgesMap::iterator iter = edges.find(tmpKmer);
		if (iter != edges.end()) {
			int size = iter->second.size();
			forn(i, size) {
				if (finishSeq->similar(*((iter->se)[i]->lower), k)) {
					if ((iter->se)[i]->used)
						return 0;
					count++;
					if (count > 1)
						return 0;
					PossibleKmer = tmpKmer;
					PossibleSequence = (iter->se)[i]->lower;
					seqIndex = i;
					PossibleIter = iter;

				}
			}
		}
	}
	if (count == 1) {
		finishKmer = (PossibleKmer) & (~(((ll) 3) << (2 * (k - 1))));
		finishSeq = PossibleSequence;
		(PossibleIter->se)[seqIndex]->used = 1;
		return 1;
	} else
		return 0;
}

int CheckUnuqueWayDown(edgesMap &edges, ll finishKmer, Sequence* finishSeq) {
	int count = 0;
	Sequence *PossibleSequence;
	ll PossibleKmer;
	for (int Nucl = 0; Nucl < 4; Nucl++) {
		ll tmpKmer = (ll) Nucl | (finishKmer << (2));
		edgesMap::iterator iter = edges.find(tmpKmer);
		if (iter != edges.end()) {
			int size = iter->second.size();
			forn(i, size) {
				if (finishSeq->similar(*((iter->se)[i]->lower), k)) {
					count++;
					if (count > 1)
						return 0;
					PossibleKmer = tmpKmer;
					PossibleSequence = (iter->se)[i]->lower;
				}
			}
		}
	}
	if (count == 1) {
		return 1;
	} else
		return 0;
}
int storeVertex(vertecesMap &verts, ll newKmer, Sequence* newSeq) {
	vertecesMap::iterator iter = verts.find(newKmer);
	if (iter != verts.end()) {
		int size = iter->second.size();
		forn(i, size) {
			if (newSeq->similar(*((iter->se)[i]->lower), k))
				return (iter->se)[i]->start;
		}
		VertexPrototype *v = new VertexPrototype();
		v->lower = newSeq;
		v->start = VertexCount;
		v->finish = 0;
		v->used = 0;
		VertexCount++;

		(iter->se).pb(v);
		return VertexCount - 1;
	} else {
		vector<VertexPrototype *> prototypes;
		VertexPrototype *v = new VertexPrototype();
		v->lower = newSeq;
		v->start = VertexCount;
		v->finish = 0;
		v->used = 0;
		VertexCount++;
		prototypes.pb(v);
		verts.insert(mp(newKmer, prototypes));
		return VertexCount - 1;
	}

}

