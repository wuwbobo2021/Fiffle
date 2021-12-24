// Figrid v0.10
// a recording software for the Five-in-a-Row game compatible with Renlib.
// By wuwbobo2021 <https://www.github.com/wuwbobo2021>, <wuwbobo@outlook.com>.
//     If you have found bugs in this program, or you have any suggestion (especially
// suggestions about adding comments), please pull an issue, or contact me. 
// Released Under GPL 3.0 License.

// Original Renlib code: <https://www.github.com/gomoku/Renlib>,
// by Frank Arkbo, Sweden. but it might be harder to understand.

#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <stack>

#include "platform_specific.h"
#include "tree.h"

using namespace std;
using namespace Namespace_Figrid;


const int Renlib_Header_Size = 20;	
//                                   0      1     2     3     4     5     6    7
unsigned char Renlib_Header[20] = { 0xFF,  'R',  'e',  'n',  'L',  'i',  'b', 0xFF,
//                                  VER    VER   10    11    12    13    14   15
                                    0x03, 0x04, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
//                                  16     17    18    19
                                    0xFF, 0xFF, 0xFF, 0xFF };

struct Renlib_Node //it corresponds with every node in a Renlib file
{
	uint8_t x: 4; // bit 0~3
	uint8_t y: 4; // bit 4~7

	//byte 2, bit 0~7
	bool extension: 1; // reserved? ignored
	bool no_move: 1;   // reserved? ignored
	bool start: 1;
	bool comment: 1;
	bool mark: 1;
	bool old_comment: 1;
	bool is_leaf: 1; //marked with right commas ): it has no left descendent. ("right" in original Renlib code)
	bool has_sibling: 1; //left commas (: it has a right sibling. ("down" in original Renlib code)
	//Reference: Data Structure Techniques by Thomas A. Standish, 3.5.2, Algorithm 3.4
};


Tree::Tree(unsigned short board_sz): board_size(board_sz), crec(board_sz) //the recording is initialized here
{
	this->proot = new Node;
	this->seq = new Node*[board_size * board_size];
	this->pos_goto_root();
}

Tree::~Tree()
{
	this->pos_goto_root();
	this->delete_current_pos();
	
	if (this->seq != NULL) {
		delete[] this->seq;
		this->seq = NULL;
	}
}

const Node* Tree::root_ptr() const
{
	return this->proot;
}

const Node* Tree::current_ptr() const
{
	return this->ppos;
}

unsigned short Tree::current_depth() const
{
	return this->cdepth;
}

unsigned short Tree::current_degree() const
{
	if (this->ppos == NULL || this->ppos->down == NULL) return 0;
	
	unsigned short cnt = 0; Node* p = this->ppos->down;
	while (p != NULL) {
		cnt++;
		p = p->right;
	}
	return cnt;
}

Move Tree::get_current_move(bool rotate_back) const
{
	Move mv;
	if (this->ppos == NULL) return mv; //return Null_Position_Move
	
	mv = this->ppos->pos;
	if (rotate_back)
		mv.rotate(this->board_size, this->tag_rotate, true); //rotate back
	return mv;
}

Recording Tree::get_current_recording(bool rotate_back) const
{
	Recording rec = this->crec;
	if (rotate_back)
		rec.board_rotate(this->tag_rotate, true);
	return rec;
}

void Tree::print_current_board(ostream& ost) const
{
	Recording rec = this->get_current_recording(true); //rotate back to meet original query
	
	Move dots[this->current_degree()];
	if (this->ppos != NULL && this->ppos->down != NULL) {
		unsigned short cnt = 0; Node* p = this->ppos->down;
		while (p != NULL) {
			dots[cnt++] = p->pos;
			dots[cnt].rotate(this->board_size, tag_rotate, true); //rotate back
			p = p->right;
		}
		rec.board_print(ost, cnt, dots);
	} else
		rec.board_print(ost);
}

bool Tree::current_mark(bool mark_start) const
{
	if (this->ppos == NULL) return false;
	
	if (mark_start)
		return this->ppos->marked_start;
	else
		return this->ppos->marked; 
}

void Tree::set_current_mark(bool val, bool mark_start)
{
	if (this->ppos == NULL) return;
	
	if (mark_start)
		this->ppos->marked_start = val;
	else
		this->ppos->marked = val;
}

bool Tree::get_current_comment(string& comment) const
{
	if (this->ppos == NULL) return false;
	if (! this->ppos->has_comment) return false;
	
	comment = this->comments[this->ppos->tag_comment];
	return true;
}

void Tree::set_current_comment(string& comment)
{
	if (this->ppos == NULL) return;
	
	if (comment != "") {
		if (this->ppos->has_comment)
			this->comments[this->ppos->tag_comment] = comment;
		else {
			this->comments.push_back(comment);
			this->ppos->has_comment = true;
			this->ppos->tag_comment = this->comments.size() - 1;
		}
	} else
		this->ppos->has_comment = false;
}
	
bool Tree::pos_move_down()
{
	if (this->ppos == NULL) return false;
	if (this->ppos->down == NULL) return false;
	
	this->ppos = this->ppos->down;
	seq[++this->cdepth] = this->ppos;
	this->crec.domove(this->ppos->pos);
	return true;
}

bool Tree::pos_move_up()
{
	if (this->cdepth < 1) return false;
	
	this->ppos = this->seq[--this->cdepth];
	this->crec.undo();
	return true;
}

bool Tree::pos_move_right()
{
	if (this->ppos == NULL) return false;
	if (this->ppos->right == NULL) return false;
	
	this->ppos = this->ppos->right;
	this->crec.undo(); this->crec.domove(this->ppos->pos);
	return true;
}

bool Tree::pos_move_left()
{
	if (this->cdepth < 1) return false;
	
	Node* p = this->seq[this->cdepth - 1];
	while (p != NULL && p->right != this->ppos)
		p = p->right;
	
	if (p == NULL || p->right != this->ppos) return false;
	this->ppos = p;
	this->crec.undo(); this->crec.domove(this->ppos->pos);
	return true;
}

void Tree::pos_goto_root()
{
	this->ppos = this->proot;
	this->seq[this->cdepth = 0] = this->proot;
	this->crec.clear();
	this->tag_rotate = Rotate_None;
}

bool Tree::pos_goto_fork()
{
	if (this->ppos == NULL) return false;
	
	while (this->ppos->down != NULL) {
		if (this->ppos->down->right != NULL) return true;
		this->ppos = this->ppos->down;
		this->seq[++this->cdepth] = this->ppos;
		this->crec.domove(this->ppos->pos);
	};
	
	return false;
}

Node* Tree::new_descendent()
{
	if (this->ppos == NULL) return NULL;
	
	if (this->ppos->down == NULL)
		return (this->ppos->down = new Node);
	else {
		Node* p = this->ppos->down;
		while (p->right != NULL)
			p = p->right;
		return p->right = new Node;
	}
}

Node* Tree::new_descendent(Move pos)
{
	Node* n = this->new_descendent();
	n->pos = pos;
	return n;
}

void Tree::delete_current_pos()
{
	if (this->ppos == NULL) return;
	bool del_root = (this->ppos == this->proot);
	
	//delete all nodes in postorder sequence
	Node* ptmp;
	stack<Node*> node_stack;
	while (1) {
		if (this->ppos->down != NULL) {
			node_stack.push(this->ppos);
			this->ppos = this->ppos->down;
		} else {
			if (this->ppos->right != NULL) { //delete current node and goto right of it
				ptmp = this->ppos->right;
				delete[] this->ppos;
				this->ppos = ptmp;
			} else { //delete current node and go up
				delete[] this->ppos;
				if (node_stack.empty()) break; //the root is deleted
				this->ppos = node_stack.top(); node_stack.pop();
				this->ppos->down = NULL; //mark
			}
		}
	}
	
	if (! del_root) {
		this->ppos = this->seq[--this->cdepth];
		this->crec.undo();
	} else { //it may happen when the tree object is being destructed, or when a library file is to be opened
		this->proot = new Node;
		this->pos_goto_root();
	}
}

Node* Tree::query(Move pos)
{
	if (this->ppos == NULL) return NULL;
	if (this->ppos->down == NULL) return NULL;
	
	Node* p = this->ppos->down;
	while (p != NULL) {
		if (p->pos == pos) {
			this->ppos = p;
			this->seq[++this->cdepth] = p;
			this->crec.domove(pos);
			return p;
		} else
			p = p->right;
	}
	
	return NULL;
}

unsigned short Tree::query(const Recording* record)
{
	if (record->moves_count() < 1) return 0;
	if (this->proot->down == NULL) return 0; //the tree is empty
	if (this->ppos == NULL) this->pos_goto_root();
	
	const Move* prec = record->recording_ptr();
	
	Node* pq = this->query(prec[0]);
	if (pq != NULL) { //the first move in this partial recording is a descendent of current position in the tree
		unsigned short mcnt;
		for (mcnt = 1; mcnt < record->moves_count(); mcnt++) {
			//check next move
			if (this->ppos->down == NULL) return mcnt;
			this->pos_move_down();
			bool found = false;
			do {
				if (prec[mcnt] == this->ppos->pos) //prec[mcnt] is the move after <mcnt>th move
					{found = true; break;}
			} while (this->pos_move_right());
			
			if (!found)
				{this->pos_move_up(); return mcnt;}
		} //mcnt++
		return mcnt;
	} else {
		if (this->ppos == this->proot) return 0;
		
		//check from root
		vector<Recording> rec; unsigned short mcnt[8];
		for (unsigned char i = 0; i < 8; i++) //create 8 empty recordings
			rec.push_back(Recording(this->board_size));
		unsigned char r = tag_rotate;
		while (r < 8) { //if tag_rotate != 0, then only execute for once
			rec[r] = *record;
			rec[r].board_rotate((Position_Rotation) r);
			this->pos_goto_root();
			mcnt[r] = this->query((& rec[r]));
			if (mcnt[r] == rec[0].moves_count()) return mcnt[r];
			
			if (this->tag_rotate != 0) return mcnt[r]; //it's already rotated last time, don't do more useless things
			r++;
		}
		
		unsigned char idx = 0;
		for (r = 0; r < 8; r++)
			if (mcnt[r] > mcnt[idx]) idx = r;
		
		this->tag_rotate = (Position_Rotation) idx;
		this->pos_goto_root();
		return this->query(& rec[idx]);
	}
}

Position_Rotation Tree::query_rotate_tag() const
{
	return this->tag_rotate;
}

void Tree::clear_rotate_tag()
{
	this->tag_rotate = Rotate_None;
}

void Tree::write_recording(const Recording* record)
{
	this->pos_goto_root();
	
	this->tag_rotate = Rotate_None; //rerotate
	unsigned short excnt = this->query(record);
	if (excnt == record->moves_count()) return;
	
	Recording rec = *record;
	rec.board_rotate(this->tag_rotate);
	const Move* prec = rec.recording_ptr();
	for (unsigned short i = excnt; i < record->moves_count(); i++) {
		this->ppos = this->new_descendent(prec[i]);
		if (this->ppos == NULL) return; //Out of memory
		this->seq[++this->cdepth] = this->ppos;
		this->crec.domove(prec[i]);
	}
}

bool Tree::is_renlib_file(const string& file_path)
{
	if (file_size(file_path) < Renlib_Header_Size + 2) return false;
	
	char head[8];
	ifstream ifs(file_path, ios_base::binary | ios_base::in);
	if (! ifs.is_open()) return false;
	ifs.read(head, 8);
	
	for (unsigned char i = 0; i < 8; i++)
		if ((unsigned char) head[i] != Renlib_Header[i]) return false;
	
	return true;
}

bool Tree::load_renlib(const string& file_path)
{
	if (this->board_size != 15) throw Invalid_Board_Size_Exception();
	if (! Tree::is_renlib_file(file_path)) return false;
	
	//make sure the loader begins with an empty tree
	this->pos_goto_root();
	this->delete_current_pos();
	this->ppos = this->proot = new Node;
	
	//read file into memory
	int lib_file_size = file_size(file_path);
	char* file_data = new char[lib_file_size];
	ifstream ifs(file_path, ios_base::binary | ios_base::in);
	if (! ifs.is_open()) return false;
	ifs.read(file_data, lib_file_size);
	ifs.close();
	
	//read the nodes in preorder to reconstruct the tree
	Renlib_Node* pread = (Renlib_Node*)(file_data + 20); //skip Renlib header

	//make sure the root has a null position
	this->new_descendent();
	this->pos_move_down();
	
	if (pread->x == 0 || pread->y == 0) //the root is a null move (so called non-stantard)
		pread++;
	
	stack<Node*> node_stack; Node* pnextpos;
	while ((char*) pread < file_data + lib_file_size) {
		pnextpos = new Node; //in which the `pos` is initialized as a null position
		if (pnextpos == NULL) {delete[] file_data; return false;} //Out of memory
		
		if (pread->x != 0 || pread->y != 0) {
			//convert x and y into Fiffle_Board::Move format, in which (0, 0) represents a1.
			this->ppos->pos.x = pread->x - 1;
			this->ppos->pos.y = 15 - pread->y - 1;
		}
		this->ppos->marked = pread->mark;
		this->ppos->marked_start = pread->start;
		
		//Reference: Data Structure Techniques by Thomas A. Standish, 3.5.2, Algorithm 3.4
		//The top of the stack will point to the parent node of the next node, which is its left descendent.
		if (pread->has_sibling)
			node_stack.push(this->ppos);
		
		//If the leaf is not the rightmost under the depth 1 subtree, it will be popped out just after being pushed in;
		//or else, it means that the parent will be popped out, and the next node will be the parent's right sibling.
		if (pread->is_leaf) {
			if (! node_stack.empty()) {
				node_stack.top()->right = pnextpos; node_stack.pop();
			} else
				break; //the program has gone through the entire tree
		} else
			this->ppos->down = pnextpos;
		
		if (pread->comment || pread->old_comment) {
			this->ppos->has_comment = true;
			
			char* pstr = (char*) (pread + 1); //the string ends with '\0'
			this->comments.push_back(pstr); //to C++ string
			this->ppos->tag_comment = this->comments.size() - 1;
			
			//goto the byte right of the last '\0'
			while (*pstr != '\0') pstr++;
			while (*pstr == '\0') pstr++;
			
			pread = (Renlib_Node*) pstr; //continue to read the next node
		} else
			pread++;
		
		this->ppos = pnextpos; //this will not happen if current node is the last node, see the 'break;' above
	}
	
	bool comp = true;
	if (this->ppos != pnextpos) //this should be true, but it prevents invalid pointer error caused by broken file
		delete pnextpos;
	else
		comp = false;
	
	delete[] file_data;
	this->pos_goto_root();
	return comp;
}

bool Tree::save_renlib(const string& file_path)
{
	if (this->board_size != 15) throw Invalid_Board_Size_Exception();
	
	if (ifstream(file_path, ios_base::binary | ios_base::in)) { //file exists
		string bak_path = file_path + ".bak";
		if (ifstream(bak_path, ios_base::binary | ios_base::in))
			remove(bak_path.c_str());
		rename(file_path.c_str(), bak_path.c_str());
	}
	
	ofstream ofs(file_path, ios_base::binary | ios_base::out);
	if (! ofs.is_open()) return false;
	ofs.write((const char*)Renlib_Header, Renlib_Header_Size);
	
	Recording rec_back = this->crec;
	
	this->ppos = this->proot;
	stack<Node*> node_stack;  Renlib_Node rnode; char* prnode = (char*) &rnode;
	if (this->proot->down != NULL && this->proot->down->pos == Move(7, 7) && this->proot->down->right == NULL)
		this->ppos = this->proot->down; //so called standard format in Renlib
	
	rnode.old_comment = rnode.no_move = rnode.extension = false; //reserved?
	
	bool tag_end = false;
	while (! tag_end) {
		if (! this->ppos->pos.position_null()) {
			rnode.x = this->ppos->pos.x + 1;
			rnode.y = 15 - (this->ppos->pos.y + 1);
			rnode.comment = this->ppos->has_comment;
			rnode.mark = this->ppos->marked;
			rnode.start = this->ppos->marked_start;
			rnode.has_sibling = (this->ppos->right != NULL);
			rnode.is_leaf = (this->ppos->down == NULL);
		} else
			prnode[0] = prnode[1] = 0x00;
		
		ofs.write((char*) &rnode, sizeof(rnode));
		if (this->ppos->has_comment) {
			string& cmt = this->comments[this->ppos->tag_comment];
			ofs.write(cmt.c_str(), cmt.length() + 1); // + 1 to write '\0'
		}
		
		//go through the nodes in preorder sequence
		if (this->ppos->right != NULL)
			node_stack.push(this->ppos);
		
		if (this->ppos->down == NULL) {
			if (! node_stack.empty()) {
				this->ppos = node_stack.top()->right; node_stack.pop();
			} else
				tag_end = true;
		} else
			this->ppos = this->ppos->down;
	}
	
	this->pos_goto_root();
	this->query(&rec_back); //recover
	return true;
}
