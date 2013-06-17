// Adjacency-list Creator for Massive Graphs in Raw Format
// Author: Wang Jia at CUHK
// Replace void NxParser() function for graphs in other raw formats (BTC by default)

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <algorithm>
#include <set>
#include <vector>
#include <map>

const int N_STRING = 100000;
const int N_SPLIT_LABEL = 5;
const int N_SPLIT_EDGE = 5;
const int N_HASH = (1 << 31) - 1;

using namespace std;

int N, M;
map<string, int> dict;
string s_raw, s_edge, s_edge_raw, s_adj;
string s_adj_in;
string s_dict[N_SPLIT_LABEL];
string s_split[N_SPLIT_EDGE];

vector< vector<int> > adj;

// tool functions

int stringHash(const char* key) {

  unsigned long h = 0;
	while (*key) {
		h = (h << 4) + *key++;
		unsigned long g = h & 0xF0000000L;
		if (g) h ^= g >> 24;
		h &= ~g;
	}
	return h & N_HASH;
}

void skipWhiteSpaces(istringstream &is_line) {

	while (isspace(is_line.peek())) is_line.get();
}

// parser for BTC (in N-quad format)

string NxGetURI(istringstream &is_line) {

	is_line.get();
	char tc;
	string s_uri = "";

	while ((tc = is_line.get()) != '>') {
		if (isspace(tc)) continue;
		s_uri += tc;
	}

	is_line.get();
	return "<" + s_uri + ">";
}

string NxGetNode(istringstream &is_line) {

	skipWhiteSpaces(is_line);
	char initial = is_line.peek();
	char tmp_c;
	string s_node = "";
	string s_language;
	string s_uriref;

	switch (initial) {
		case '<':
			s_node = NxGetURI(is_line);
			break;

		case '_':
			is_line.get();
			is_line.get();
			is_line >> s_node;
			s_node = "_:"+s_node;
			break;

		case '\"':
			is_line.get();
			while ((tmp_c = is_line.get()) != '\"') {
				if (tmp_c == ' ')
					tmp_c = '_';
				else if (tmp_c == '\\') {
					tmp_c = is_line.get();
					if (tmp_c == 'u')
						for (int i=0; i<4; ++i) is_line.get();
					else if (tmp_c == 'U')
						for (int i=0; i<8; ++i) is_line.get();
				}
				else
					s_node += tmp_c;
			}
			s_node = '\"' + s_node + "\"";
			if (is_line.peek() == '@') {
				s_node += is_line.get();
				is_line >> s_language;
				s_node += s_language;
			}
			else if(is_line.peek() == '^') {
				s_node += is_line.get();
				s_node += is_line.get();
				s_uriref = NxGetURI(is_line);
				s_node += s_uriref;
			}
			break;

		case '.':
			s_node = ".";
			break;

		default:
			s_node = "*";
			break;
	}

	if (s_node == "") s_node = '*';
	return s_node;
}

void NxParse() {

	ifstream f_raw(s_raw.c_str());
	istringstream is_line;
	string line;
	string s_subject, s_predicate, s_object, s_unknown;

	FILE *f_edge_raw = fopen(s_edge_raw.c_str(), "w");
	FILE *f_dicts[N_SPLIT_LABEL];
	for (int i=0; i<N_SPLIT_LABEL; ++i)
		f_dicts[i] = fopen(s_dict[i].c_str(), "w");

	int cnt = 0;
	while (getline(f_raw, line)) {

		++cnt;
		is_line.str(line);

		s_subject = NxGetNode(is_line);
		if (s_subject == "#") continue;

		s_predicate = NxGetNode(is_line);
		s_object = NxGetNode(is_line);

		s_unknown = NxGetNode(is_line);
		if (s_unknown != ".") {
			s_unknown = NxGetNode(is_line);
			if (s_unknown != ".") {
				fprintf(stderr, "Wrong #items @line%d.\n", cnt);
				exit(0);
			}
		}

		// output parsed labels & edges
		int f1 = stringHash(s_subject.c_str()) % N_SPLIT_LABEL;
		int f2 = stringHash(s_object.c_str()) % N_SPLIT_LABEL;
		fprintf(f_dicts[f1], "%s\n", s_subject.c_str());
		fprintf(f_dicts[f2], "%s\n", s_object.c_str());
		fprintf(f_edge_raw, "%s %s\n", s_subject.c_str(), s_object.c_str());
	}

	fclose(f_edge_raw);
	for (int i=0; i<N_SPLIT_LABEL; ++i)
		fclose(f_dicts[i]);
}

// create map from label to id
void makeDict(FILE *f_dict) {

	char c_label[N_STRING];
	memset(c_label, 0, sizeof(c_label));
	dict.clear();

	while (fscanf(f_dict, "%s", c_label) == 1) {
		if (strlen(c_label) == 0) break;
		string label(c_label);
		if (dict.find(label) == dict.end()) {
			dict[label] = N++;
		}
		memset(c_label, 0, sizeof(c_label));
	}
}

// map label to id, if label exists in memory
inline void doLabel2Id(FILE *f_new, const string &s) {

	if (isdigit(s[0]))
		fprintf(f_new, "%s", s.c_str());
	else if (dict.find(s) != dict.end())
		fprintf(f_new, "%d", dict[s]);
	else
		fprintf(f_new, "%s", s.c_str());
}

// map labels in f_old to ids stored in f_new,  when mapping exists in-memory
void label2Id(FILE *f_old, FILE *f_new) {

	char c_v1[N_STRING];
	char c_v2[N_STRING];

	while (fscanf(f_old, "%s", c_v1) == 1) {
		fscanf(f_old, "%s", c_v2);
		string v1(c_v1);
		string v2(c_v2);
		doLabel2Id(f_new, v1); fprintf(f_new, " "); doLabel2Id(f_new, v2); fprintf(f_new, "\n");
	}
}

// convert edges in labels list to those in ids
void makeEdgeList() {

	FILE *f_old, *f_dict, *f_new;
	string f_tmp_0 = s_raw + "_tmp_0";
	string f_tmp_1 = s_raw + "_tmp_1";

	for (int fid=0; fid<N_SPLIT_LABEL; ++fid) {

		int o_old = (fid & 1);
		int o_new = 1-o_old;
		string s_old = s_raw + "_tmp_" + char(o_old+'0');
		string s_new = s_raw + "_tmp_" + char(o_new+'0');
		f_old = fopen(s_old.c_str(), "r");
		f_new = fopen(s_new.c_str(), "w");
		f_dict = fopen(s_dict[fid].c_str(), "r");

		makeDict(f_dict);
		label2Id(f_old, f_new);

		fclose(f_old);
		fclose(f_new);
		fclose(f_dict);

		if (fid == N_SPLIT_LABEL-1)
			s_adj_in = s_new;
	}
}

// find the split for vertex v
inline int findSplit(int v) {

	return v / ((N-1)/N_SPLIT_EDGE+1);
}

// output edge (v1,v2) to v1's split
inline void outputEdge(FILE **f_split, int v1, int v2) {

	fprintf(f_split[findSplit(v1)], "%d %d\n", v1, v2);
}

// split edge list
void makeSplits() {

	FILE *f_adj_in = fopen(s_adj_in.c_str(), "r");
	FILE *f_split[N_SPLIT_EDGE];

	for (int i=0; i<N_SPLIT_EDGE; ++i)
		f_split[i] = fopen(s_split[i].c_str(), "w");

	int v1, v2;

	while (fscanf(f_adj_in, "%d", &v1) ==1) {
		fscanf(f_adj_in, "%d", &v2);
		outputEdge(f_split, v1, v2);
		outputEdge(f_split, v2, v1);
	}

	fclose(f_adj_in);
	for (int i=0; i<N_SPLIT_EDGE; ++i)
		fclose(f_split[i]);
}

// merge splits
void mergeSplits() {

	FILE *f_split;
	FILE *f_adj = fopen(s_adj.c_str(),"w");

	int sz = (N-1)/N_SPLIT_EDGE+1;
	adj.resize(sz);

	for (int fid=0; fid<N_SPLIT_EDGE; ++fid) {

		f_split = fopen(s_split[fid].c_str(), "r");

		adj.clear();
		for (int i=0; i<sz; ++i) adj[i].clear();

		int v1, v2;
		int start_v = fid*sz;
		while (fscanf(f_split, "%d", &v1) ==1) {
			fscanf(f_split, "%d", &v2);
			adj[v1-start_v].push_back(v2);
		}

		fprintf(f_adj, "%d\n", N);
		for (int i=0; i<sz; ++i) {
			if (adj[i].size() == 0)
				continue;
			sort(adj[i].begin(), adj[i].end());
			vector<int>::iterator it = unique(adj[i].begin(), adj[i].end());
			adj[i].resize(it-adj[i].begin());
			fprintf(f_adj, "%d %d", i+start_v, adj[i].size());
			M += adj[i].size();
			for (unsigned j=0; j<adj[i].size(); ++j)
				fprintf(f_adj, " %d", adj[i][j]);
			fprintf(f_adj, "\n");
		}

		fclose(f_split);
	}

	fclose(f_adj);
}

int main(int argc, char **argv) {

	if (argc != 2)
		exit(1);

	s_raw = argv[1];
	s_edge = s_raw + "_edge";
	s_edge_raw = s_raw + "_tmp_0";
	s_adj = s_raw + "_adj";
	for (int i=0; i<N_SPLIT_LABEL; ++i)
		s_dict[i] = s_raw + "_dict_" + char('a'+i);
	for (int i=0; i<N_SPLIT_EDGE; ++i)
		s_split[i] = s_raw + "_split_" + char('a'+i);

	NxParse();
	cout << "mark\n";
	makeEdgeList();
	makeSplits();
	mergeSplits();

	cout << "n = " << N << "\nm = " << M << endl;

	return 0;
}
