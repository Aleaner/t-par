/*--------------------------------------------------------------------
  Tpar - T-gate optimization for quantum circuits
  Copyright (C) 2013  Matthew Amy and The University of Waterloo,
  Institute for Quantum Computing, Quantum Circuits Group

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

Author: Matthew Amy
---------------------------------------------------------------------*/

#include "circuit.h"
#include <algorithm>
#include <sstream>

//----------------------------------------- DOTQC stuff

void ignore_white(istream& in) {
  while (in.peek() == ' ' || in.peek() == ';') in.ignore();
}

void dotqc::input(istream& in) {
  int i, j;
  string buf, tmp;
  list<string> namelist;
  n = 0;

  // Inputs
  while (buf != ".v") in >> buf;
  ignore_white(in);
  while(in.peek() != '\n' && in.peek() != '\r') {
    in >> buf;
    names.push_back(buf);
    zero[buf] = 1;
    ignore_white(in);
  }

  // Primary inputs
  while (buf != ".i") in >> buf;
  ignore_white(in);
  while (in.peek() != '\n' && in.peek() != '\r') {
    n++;
    in >> buf;
    zero[buf] = 0;
    ignore_white(in);
  }

  m = names.size() - n;

  // Circuit
  while (buf != "BEGIN") in >> buf;
  in >> tmp;
  while (tmp != "END") {
    namelist.clear();
    // Build up a list of the applied qubits
    ignore_white(in);
    while (in.peek() != '\n' && in.peek() != '\r' && in.peek() != ';') {
      in >> buf;
      int pos = buf.find(';');
      if (pos != string::npos) {
        for (int i = buf.length() - 1; i > pos; i--) {
          in.putback(buf[i]);
        }
        in.putback('\n');
        buf.erase(pos, buf.length() - pos);
      }
      if (find(names.begin(), names.end(), buf) == names.end()) {
        cout << "ERROR: no such qubit \"" << buf << "\"\n" << flush;
        exit(1);
      } else {
        namelist.push_back(buf);
      }
      ignore_white(in);
    }
    if (tmp == "TOF") tmp = "tof";
    circ.push_back(make_pair(tmp, namelist));
    in >> tmp;
  }
}

void dotqc::output(ostream& out) {
  int i;
  list<string>::iterator name_it;
  gatelist::iterator it; 
  list<string>::iterator ti;

  // Inputs
  out << ".v";
  for (name_it = names.begin(); name_it != names.end(); name_it++) {
    out << " " << *name_it;
  }

  // Primary inputs
  out << "\n.i";
  for (name_it = names.begin(); name_it != names.end(); name_it++) {
    if (zero[*name_it] == 0) out << " " << *name_it;
  }

  // Outputs
  out << "\n.o";
  for (name_it = names.begin(); name_it != names.end(); name_it++) {
    out << " " << *name_it;
  }

  // Circuit
  out << "\n\nBEGIN\n";
  for (it = circ.begin(); it != circ.end(); it++) {
    out << it->first;
    for (ti = (it->second).begin(); ti != (it->second).end(); ti++) {
      out << " " << *ti;
    }
    out << "\n";
  }
  out << "END\n";
}

int max_depth(map<string, int> & depths, list<string> & names) {
  list<string>::const_iterator it;
  int max = 0;

  for (it = names.begin(); it != names.end(); it++) {
    if (depths[*it] > max) max = depths[*it];
  }

  return max;
}

// Compute T-depth
int dotqc::count_t_depth() {
  gatelist::reverse_iterator it;
  list<string>::const_iterator ti;
  map<string, int> current_t_depth;
  int d;

  for (ti = names.begin(); ti != names.end(); ti++) {
    current_t_depth[*ti] = 0;
  }

  for (it = circ.rbegin(); it != circ.rend(); it++) {
    d = max_depth(current_t_depth, it->second);
    if ((it->first == "T") || (it->first == "T*")) {
      d = d + 1;
    } else if ((it->first == "Z") && (it->second.size() >= 3)) {
      d = d + 3;
    }
    for (ti = it->second.begin(); ti != it->second.end(); ti++) {
      current_t_depth[*ti] = d;
    }
  }

  return max_depth(current_t_depth, names);
}

// Gather statistics and print
void dotqc::print_stats() {
  int H = 0;
  int cnot = 0;
  int X = 0;
  int T = 0;
  int P = 0;
  int Z = 0;
  int tdepth = 0;
  bool tlayer = false;
  gatelist::iterator ti;
  set<string> qubits;
  list<string>::iterator it;

  for (ti = circ.begin(); ti != circ.end(); ti++) {
    for (it = ti->second.begin(); it != ti->second.end(); it++) qubits.insert(*it);
    if (ti->first == "T" || ti->first == "T*") {
      T++;
      if (!tlayer) {
        tlayer = true;
        tdepth++;
      }
    } else if (ti->first == "P" || ti->first == "P*") P++;
    else if (ti->first == "Z" && ti->second.size() == 3) {
      tdepth += 3;
      T += 7;
      cnot += 7;
    } else if (ti->first == "Z") Z++;
    else {
      if (ti->first == "tof" && ti->second.size() == 2) cnot++;
      else if (ti->first == "tof" || ti->first == "X") X++;
      else if (ti->first == "H") H++;

      if (tlayer) tlayer = false;
    }
  }

  cout << "#   qubits: " << names.size() << "\n";
  cout << "#   qubits used: " << qubits.size() << "\n";
  cout << "#   H: " << H << "\n";
  cout << "#   cnot: " << cnot << "\n";
  cout << "#   X: " << X << "\n";
  cout << "#   T: " << T << "\n";
  cout << "#   P: " << P << "\n";
  cout << "#   Z: " << Z << "\n";
  cout << "#   tdepth (by partitions): " << tdepth << "\n";
  cout << "#   tdepth (by critical paths): " << count_t_depth() << "\n";

}

// Count the Hadamard gates
int count_h(dotqc & qc) {
  int ret = 0;
  list<pair<string,list<string> > >::iterator it;

  for (it = qc.circ.begin(); it != qc.circ.end(); it++) {
    if (it->first == "H") ret++;
  }

  return ret;
}

bool find_name(const list<string> & names, const string & name) {
  list<string>::const_iterator it;
  for (it = names.begin(); it != names.end(); it++) {
    if (*it == name) return true;
  }
  return false;
}

void dotqc::append(pair<string, list<string> > gate) {
  circ.push_back(gate);

  for (list<string>::iterator it = gate.second.begin(); it != gate.second.end(); it++) {
    if (!find_name(names, *it)) names.push_back(*it);
  }
}

// Optimizations
void dotqc::remove_swaps() {
  gatelist::iterator it, tt, ttt;
  list<string>::iterator iti;
  map<string, string>::iterator pit;
  string q1, q2, q1_map, q2_map, tmp;
  bool flg, found_q1, found_q2;
  int i;
  map<string, string> perm;

  for (it = circ.begin(), i = 0; i < (circ.size() - 3);) {
    flg = false;
    if (it->first == "tof" && it->second.size() == 2) {
      iti = it->second.begin();
      q1 = *(iti++);
      q2 = *iti;

      tt = it;
      tt++;
      ttt = tt;
      ttt++;
      if (tt->first == "tof" && tt->second.size() == 2 && ttt->first == "tof" && ttt->second.size() == 2) {
        flg = true;
        iti = tt->second.begin();
        flg &= *(iti++) == q2;
        flg &= *(iti) == q1;
        iti = ttt->second.begin();
        flg &= *(iti++) == q1;
        flg &= *(iti) == q2;

        if (flg) {
          circ.erase(it);
          circ.erase(tt);
          it = circ.erase(ttt);
          found_q1 = found_q2 = false;
          q1_map = (perm.find(q2) != perm.end()) ? perm[q2] : q2;
          q2_map = (perm.find(q1) != perm.end()) ? perm[q1] : q1;
          perm[q1] = q1_map;
          perm[q2] = q2_map;
        }
      }
    }
    if (!flg) {
      // Apply permutation
      for (iti = it->second.begin(); iti != it->second.end(); iti++) {
        if (perm.find(*iti) != perm.end()) *iti = perm[*iti];
      }
      i++;
      it++;
    }
  }
  for (; i < circ.size(); i++) {
    for (iti = it->second.begin(); iti != it->second.end(); iti++) {
      if (perm.find(*iti) != perm.end()) *iti = perm[*iti];
    }
    it++;
  }

  // fix outputs
  while(!perm.empty()) {
    pit = perm.begin();
    if (pit->first == pit->second) perm.erase(pit);
    else {
      list<string> tmp_list1, tmp_list2;
      q1 = pit->second;
      q2 = perm[pit->second];
      tmp_list1.push_back(q1);
      tmp_list1.push_back(q2);
      tmp_list2.push_back(q2);
      tmp_list2.push_back(q1);
      circ.push_back(make_pair("tof", tmp_list1));
      circ.push_back(make_pair("tof", tmp_list2));
      circ.push_back(make_pair("tof", tmp_list1));

      pit->second = q2;
      perm[q1] = q1;
    }
  }
}

int list_compare(const list<string> & a, const list<string> & b) {
  list<string>::const_iterator it, ti;
  bool disjoint = true, equal = true, elt, strongelt;
  int i = a.size(), j = b.size();

  for (it = a.begin(), i = 0; i < a.size(); i++, it++) {
    elt = false;
    strongelt = false;
    for (ti = b.begin(), j = 0; j < b.size(); j++, ti++) {
      if (*it == *ti) {
        elt = true;
        disjoint = false;
        if (i == j) {
          strongelt = true;
        }
      }
    }
    if (!strongelt) equal = false;
  }

  if (equal) return 3;
  else if (!disjoint) return 2;
  else return 1;
}

void dotqc::remove_ids() {
  gatelist::iterator it, ti;
  bool mod = true, flg = true;
  int i;
  map<string, string> ids;

  ids["tof"] = "tof";
  ids["Z"] = "Z";
  ids["H"] = "H";
  ids["P"] = "P*";
  ids["P*"] = "P";
  ids["T"] = "T*";
  ids["T*"] = "T";

  while (mod) {
    mod = false;
    for (it = circ.begin(); it != circ.end(); it++) {
      flg = false;
      ti = it;
      ti++;
      for (; ti != circ.end() && !flg; ti++) {
        i = list_compare(it->second, ti->second);
        switch (i) {
          case 3:
            if (ids[it->first] == ti->first) {
              ti = circ.erase(ti);
              it = circ.erase(it);
              mod = true;
            }
            flg = true;
            break;
          case 2:
            flg = true;
            break;
          default:
            break;
        }
      }
    }
  }
}

//-------------------------------------- End of DOTQC stuff

void character::output(ostream& out) {
  int i, j;
  bool flag;
  vector<exponent>::iterator it;

  out << "U|";
  for (i = 0; i < (n + m); i++) {
    if (i != 0)  out << " ";
    if (zero[i]) out << "()";
    else out << names[i];
  }

  out << "> --> w^(";

  // Print the phase exponents
  for (it = phase_expts.begin(); it != phase_expts.end(); it++) {
    if (it != phase_expts.begin()) out << "+";
    out << (int)(it->first) << "*";
    if (it->second.test(n + h)) out << "~";
    for (i = 0; i < (n + h); i++) {
      if (it->second.test(i)) out << names[val_map[i]];
    }
  }
  out << ")|";

  // Print the output functions
  for (i = 0; i < (n + m); i++) {
    flag = false;
    out << "(";
    if (outputs[i].test(n + h)) out << "~";
    for (j = 0; j < (n + h); j++) {
      if (outputs[i].test(j)) {
        if (flag) out << " ";
        out << names[val_map[j]];
        flag = true;
      }
    }
    out << ")";
  }
  out << ">\n";

  // Print the Hadamards:
  for (list<Hadamard>::iterator ti = hadamards.begin(); ti != hadamards.end(); ti++) {
    out << "H:" << names[ti->qubit] << "-->" << ti->prep << "\n";
  }
}

// The code is all fucked anyway, might as well just hack this in
void insert_phase (pair<string, int> ph, xor_func f, 
		   map<string, pair<int, vector<exponent> > > & phases) {

  vector<exponent>::iterator it;
  phases[ph.first].first = max(ph.second, phases[ph.first].first);
  int diff = ph.second - phases[ph.first].first;
  char val;
  int cor;
  if (diff >= 0) {
    phases[ph.first].first = ph.second;
    val = 1;
    cor = diff;
  } else {
    val = 1 << abs(diff);
    cor = 0;
  }

  bool flg = false;
  for (it = phases[ph.first].second.begin(); it != phases[ph.first].second.end(); it++) {
    // renormalize
    it->first = it->first << cor;
    // Add the phase
    if (it->second == f) {
      if (ph.first[0] == '-') {
	it->first = it->first - val;
      } else {
	it->first = it->first + val;
      }
    }
  }
  if (!flg) {
    phases[ph.first].second.push_back(make_pair(val, xor_func(f)));
  }
}

pair<gate, pair<string, int> > parse_gate(const string & s) {
  string gate;
  string base = "";
  int exp2 = 0;

  size_t br = s.find('(');
  if (br != string::npos) {
    gate = s.substr(0, br - 1);
    base = s.substr(br+1, s.find('/') - 1);
    istringstream(s.substr(s.find('^') + 1, s.find(')') - 1)) >> exp2;
  } else {
    gate = s;
  }

  return make_pair(base, exp2);
}

vector<int> parse_args(const list<string> & args, const map<string, int> names) {
  vector<int> ret;

  for (list<string>::const_iterator it = args.begin(); it != args.end(); it++) {
    ret.push_back(names[*it]);
  }

  return ret;
}

// Parse a {CNOT, T} circuit
// NOTE: a qubit's number is NOT the same as the bit it's value represents
void character::parse_circuit(dotqc & input) {
  int i, j, a, b, c, name_max = 0, val_max = 0;
  n = input.n;
  m = input.m;
  h = count_h(input);

  hadamards.clear();
  map<string, int> name_map;
  pair<string, pair<string, int> > gate;
  vector<int> qbits;

  // Initialize names and wires
  names = new string [n + m + h];
  zero  = new bool   [n + m];
  xor_func * wires = outputs = new xor_func [n + m];
  for (list<string>::iterator it = input.names.begin(); it != input.names.end(); it++) {
    // name_map maps a name to a wire
    name_map[*it] = name_max;
    // names maps a wire to a name
    names[name_max] = *it;
    // zero mapping
    zero[name_max]  = input.zero[*it];
    // each wire has an initial value j, unless it starts in the 0 state
    wires[name_max] = xor_func(n + h + 1, 0);
    if (!zero[name_max]) {
      wires[name_max].set(val_max);
      val_map[val_max++] = name_max;
    }
    name_max++;
  }

  bool flg;

  gatelist::iterator it;
  for (it = input.circ.begin(); it != input.circ.end(); it++) {
    gate = parse_gate(it->first);
    qbits = parse_args(it->second, name_map);

    if (gate == "tof" && qbits.size() == 1) gate = "X";
    else if (gate == "Z" && qbits.size() == 3) gate = "Z3";

    flg = false;
    switch (gate->first) {
    // Tedious cases for phase gates.............
    case "Rz":
      insert_phase(gate->second, wires[qbits[0]], phase_expts);
      break;
    case "T":
      insert_phase(make_pair("pi", 2), wires[qbits[0]], phase_expts);
      break;
    case "T*":
      insert_phase(make_pair("-pi", 2), wires[qbits[0]], phase_expts);
      break;
    case "P":
      insert_phase(make_pair("pi", 1), wires[qbits[0]], phase_expts);
      break;
    case "P*":
      insert_phase(make_pair("-pi", 1), wires[qbits[0]], phase_expts);
      break;
    case "Z":
      insert_phase(make_pair("pi", 0), wires[qbits[0]], phase_expts);
      break;
    // Wow an X gate
    case "X":
      wires[qbits[0]].flip(n + h);
      break;
    case "Y":
      wires[qbits[0]].flip(n + h);
      insert_phase(make_pair("pi", 0), wires[qbits[0]], phase_expts);
      break;
    // cnot name tof for some reason
    case "tof":
      wires[qbits[1]] ^= wires[qbits[0]];
      break;
    case "Z3":
      insert_phase(make_pair("pi", 2), wires[qbits[0]], phase_expts);
      insert_phase(make_pair("pi", 2), wires[qbits[1]], phase_expts);
      insert_phase(make_pair("pi", 2), wires[qbits[2]], phase_expts);
      insert_phase(make_pair("-pi", 2), wires[qbits[0]] ^ wires[qbits[1]], phase_expts);
      insert_phase(make_pair("-pi", 2), wires[qbits[0]] ^ wires[qbits[2]], phase_expts);
      insert_phase(make_pair("-pi", 2), wires[qbits[1]] ^ wires[qbits[2]], phase_expts);
      insert_phase(make_pair("pi", 2), wires[qbits[0]] ^ wires[qbits[1]] ^ wires[qbits[2]], phase_expts);
    // the interesting case
    case "H":
      Hadamard new_h;
      new_h.qubit = qbits[0];
      new_h.prep  = val_max++;
      new_h.wires = new xor_func[n + m];
      for (i = 0; i < n + m; i++) {
        new_h.wires[i] = wires[i];
      }

      // Check previous exponents to see if they're inconsistent
      wires[new_h.qubit].reset();
      int rank = compute_rank(n + m, n + h, wires);
      for (map<string, pair<int, vector<exponent> > >::iterator it = phase_expts.begin();
	   it != phase_expts.end(); it++) {
	vector<exponent> * tmp = &((it->second).second);
	for (i = 0; i < tmp->size(); i++) {
	  if ((*tmp)[i].first != 0) {
	    wires[new_h.qubit] = (*tmp)[i].second;
	    if (compute_rank(n + m, n + h, wires) > rank) new_h.in[it->first].insert(i);
	  }
	}
      }

      // Done creating the new hadamard
      hadamards.push_back(new_h);

      // Prepare the new value
      wires[new_h.qubit].reset();
      wires[new_h.qubit].set(new_h.prep);

      // Give this new value a name
      val_map[new_h.prep] = name_max;
      names[name_max] = names[new_h.qubit];
      names[name_max++].append(to_string(new_h.prep));
      break;
    case default:
      cout << "ERROR: not a valid circuit\n";
      delete[] outputs;
    }
  }
}

//---------------------------- Synthesis

dotqc character::synthesize() {
  partitioning floats[2], frozen[2]; 
  dotqc ret;
  xor_func mask(n + h + 1, 0);      // Tells us what values we have prepared
  xor_func wires[n + m];        // Current state of the wires
  list<int> remaining[2];          // Which terms we still have to partition
  int dim = n, tmp, tdepth = 0, h_count = 1, applied = 0, j;
  ind_oracle oracle(n + m, dim, n + h);
  list<pair<string, list<string> > > circ;
  list<Hadamard>::iterator it;

  // initialize some stuff
  ret.n = n;
  ret.m = m;
  mask.set(n + h);
  for (int i = 0, j = 0; i < n + m; i++) {
    ret.names.push_back(names[i]);
    ret.zero[names[i]] = zero[i];
    wires[i] = xor_func(n + h + 1, 0);
    if (!zero[i]) {
      wires[i].set(j);
      mask.set(j++);
    }
  }

  // initialize the remaining list
  for (int i = 0; i < phase_expts.size(); i++) {
    if (phase_expts[i].first % 2 == 1) remaining[0].push_back(i);
    else if (phase_expts[i].first != 0) remaining[1].push_back(i);
  }

  // create an initial partition
  // cerr << "Adding new functions to the partition... " << flush;
  for (j = 0; j < 2; j++) {
    for (list<int>::iterator it = remaining[j].begin(); it != remaining[j].end();) {
      xor_func tmp = (~mask) & (phase_expts[*it].second);
      if (tmp.none()) {
        add_to_partition(floats[j], *it, phase_expts, oracle);
        it = remaining[j].erase(it);
      } else it++;
    }
  }
  if (disp_log) cerr << "  " << phase_expts.size() - (remaining[0].size() + remaining[1].size())  
    << "/" << phase_expts.size() << " phase rotations partitioned\n" << flush;

  for (it = hadamards.begin(); it != hadamards.end(); it++, h_count++) {
    // 1. freeze partitions that are not disjoint from the hadamard input
    // 2. construct CNOT+T circuit
    // 3. apply the hadamard gate
    // 4. add new functions to the partition
    if (disp_log) cerr << "  Hadamard " << h_count << "/" << hadamards.size() << "\n" << flush;

    // determine frozen partitions
    for (j = 0; j < 2; j++) {
      frozen[j] = freeze_partitions(floats[j], it->in);
      applied += num_elts(frozen[j]);
    }

    // Construct {CNOT, T} subcircuit for the frozen partitions
    ret.circ.splice(ret.circ.end(), 
        construct_circuit(phase_expts, frozen[0], wires, wires, n + m, n + h, names));
    ret.circ.splice(ret.circ.end(), 
        construct_circuit(phase_expts, frozen[1], wires, it->wires, n + m, n + h, names));
    for (int i = 0; i < n + m; i++) {
      wires[i] = it->wires[i];
    }
    if (disp_log) cerr << "    " << applied << "/" << phase_expts.size() << " phase rotations applied\n" << flush;

    // Apply Hadamard gate
    ret.circ.push_back(make_pair("H", list<string>(1, names[it->qubit])));
    wires[it->qubit].reset();
    wires[it->qubit].set(it->prep);
    mask.set(it->prep);

    // Check for increases in dimension
    tmp = compute_rank(n + m, n + h, wires);
    if (tmp > dim) {
      if (disp_log) cerr << "    Dimension increased to " << tmp << ", fixing partitions...\n" << flush;
      dim = tmp;
      oracle.set_dim(dim);
      repartition(floats[0], phase_expts, oracle);
      repartition(floats[1], phase_expts, oracle);
    }

    // Add new functions to the partition
    for (j = 0; j < 2; j++) {
      for (list<int>::iterator it = remaining[j].begin(); it != remaining[j].end();) {
        xor_func tmp = (~mask) & (phase_expts[*it].second);
        if (tmp.none()) {
          add_to_partition(floats[j], *it, phase_expts, oracle);
          it = remaining[j].erase(it);
        } else it++;
      }
    }
    if (disp_log) cerr << "    " << phase_expts.size() - (remaining[0].size() + remaining[1].size())
      << "/" << phase_expts.size() << " phase rotations partitioned\n" << flush;
  }

  applied += num_elts(floats[0]) + num_elts(floats[1]);
  // Construct the final {CNOT, T} subcircuit
  ret.circ.splice(ret.circ.end(), 
      construct_circuit(phase_expts, floats[0], wires, wires, n + m, n + h, names));
  ret.circ.splice(ret.circ.end(), 
      construct_circuit(phase_expts, floats[1], wires, outputs, n + m, n + h, names));
  if (disp_log) cerr << "  " << applied << "/" << phase_expts.size() << " phase rotations applied\n" << flush;

  return ret;
}

//-------------------------------- old {CNOT, T} version code. Still used for the "no hadamards" option

void metacircuit::partition_dotqc(dotqc & input) {
  list<pair<string, list<string> > >::iterator it;
  list<string>::iterator iti;
  map<string, bool>::iterator ti;
  circuit_type current = UNKNOWN;

  n = input.n;
  m = input.m;
  circuit_list.clear();
  names = input.names;
  zero  = input.zero;

  dotqc acc;
  map<string, bool> zero_acc = input.zero;

  acc.zero = zero_acc;
  for (it = input.circ.begin(); it != input.circ.end(); it++) {
    if ((it->first == "T"   && it->second.size() == 1) ||
        (it->first == "T*"  && it->second.size() == 1) ||
        (it->first == "P"   && it->second.size() == 1) ||
        (it->first == "P*"  && it->second.size() == 1) ||
        (it->first == "X"   && it->second.size() == 1) ||
        (it->first == "Y"   && it->second.size() == 1) ||
        (it->first == "Z"   && (it->second.size() == 1 || it->second.size() == 3)) ||
        (it->first == "tof" && (it->second.size() == 1 || it->second.size() == 2))) {
      if (current == UNKNOWN) {
        current = CNOTT;
      } else if (current != CNOTT) {
        acc.n = acc.m = 0;
        for (ti = acc.zero.begin(); ti != acc.zero.end(); ti++) {
          if (ti->second) {
            acc.m += 1;
            if (!find_name(acc.names, ti->first)) acc.names.push_back(ti->first);
          }
        }
        acc.n = acc.names.size() - acc.m;
        circuit_list.push_back(make_pair(current, acc));

        acc.clear();
        acc.zero = zero_acc;
        current = CNOTT;
      }
    } else {
      if (current == UNKNOWN) {
        current = OTHER;
      } else if (current != OTHER) {
        acc.n = acc.m = 0;
        for (ti = acc.zero.begin(); ti != acc.zero.end(); ti++) {
          if (ti->second) {
            acc.m += 1;
            if (!find_name(acc.names, ti->first)) acc.names.push_back(ti->first);
          }
        }
        acc.n = acc.names.size() - acc.m;
        circuit_list.push_back(make_pair(current, acc));

        acc.clear();
        acc.zero = zero_acc;
        current = OTHER;
      }
    }
    for (iti = it->second.begin(); iti != it->second.end(); iti++) {
      zero_acc[*iti] = false;
    }
    acc.append(*it);
  }

  acc.n = acc.m = 0;
  for (ti = acc.zero.begin(); ti != acc.zero.end(); ti++) {
    if (ti->second) {
      acc.m += 1;
      if (!find_name(acc.names, ti->first)) acc.names.push_back(ti->first);
    }
  }
  acc.n = acc.names.size() - acc.m;
  circuit_list.push_back(make_pair(current, acc));
}

void metacircuit::output(ostream& out) {
  list<pair<circuit_type, dotqc> >::iterator it;
  for (it = circuit_list.begin(); it != circuit_list.end(); it++) {
    if (it->first == CNOTT) {
      character tmp;
      tmp.parse_circuit(it->second);
      out << "CNOT, T circuit: " << tmp.n << " " << tmp.m << "\n";
      tmp.output(out);
    }
    else out << "Other: " << it->second.n << " " << it->second.m << "\n";
    it->second.output(out);
    out << "\n";
  }
}

dotqc metacircuit::to_dotqc() {
  dotqc ret;
  list<pair<circuit_type, dotqc> >::iterator it;
  gatelist::iterator ti;
  ret.n = n;
  ret.m = m;
  ret.names = names;
  ret.zero = zero;

  for (it = circuit_list.begin(); it != circuit_list.end(); it++) {
    for (ti = it->second.circ.begin(); ti != it->second.circ.end(); ti++) {
      ret.circ.push_back(*ti);
    }
  }

  return ret;
}

void metacircuit::optimize() {
  list<pair<circuit_type, dotqc> >::iterator it;
  for (it =circuit_list.begin(); it != circuit_list.end(); it++) {
    if (it->first == CNOTT) {
      character tmp;
      tmp.parse_circuit(it->second);
      it->second = tmp.synthesize();
    }
  }
}
