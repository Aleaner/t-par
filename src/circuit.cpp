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
}

// The code is all fucked anyway, might as well just hack this in
void insert_phase (pair<string, int> ph, xor_func f, 
    map<string, pair<int, vector<exponent> > > & phases) {
  // Strip the "-"
  bool minus = (ph.first[0] == '-');
  if (minus) ph.first = ph.first.substr(1);

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
  if (minus) val = -val;

  bool flg = false;
  for (it = phases[ph.first].second.begin(); it != phases[ph.first].second.end(); it++) {
    // renormalize
    it->first = it->first << cor;
    // Add the phase
    if (it->second == f) {
      it->first = it->first + val;
      flg = true;
    }
  }
  if (!flg) {
    phases[ph.first].second.push_back(make_pair(val, xor_func(f)));
  }
}

pair<string, pair<string, int> > parse_gate(const string & s) {
  string gate;
  string base = "";
  int exp2 = 0;

  size_t br = s.find('(');
  if (br != string::npos) {
    gate = s.substr(0, br);
    base = s.substr(br+1, s.find('/') - br - 1);
    istringstream(s.substr(s.find('^') + 1, s.find(')') - 1)) >> exp2;
  } else {
    gate = s;
  }

  return make_pair(gate, make_pair(base, exp2));
}

vector<int> parse_args(list<string> & args, map<string, int> names) {
  vector<int> ret;

  for (list<string>::iterator it = args.begin(); it != args.end(); it++) {
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

    if (gate.first == "tof" && qbits.size() == 1) gate.first = "X";
    else if (gate.first == "Z" && qbits.size() == 3) gate.first = "Z3";

    flg = false;
    // Tedious cases for phase gates.............
    if (gate.first == "Rz") {
      insert_phase(gate.second, wires[qbits[0]], phase_expts);
    } else if (gate.first == "T") {
      insert_phase(make_pair("pi", 2), wires[qbits[0]], phase_expts);
    } else if (gate.first == "T*") {
      insert_phase(make_pair("-pi", 2), wires[qbits[0]], phase_expts);
    } else if (gate.first == "P") {
      insert_phase(make_pair("pi", 1), wires[qbits[0]], phase_expts);
    } else if (gate.first == "P*") {
      insert_phase(make_pair("-pi", 1), wires[qbits[0]], phase_expts);
    } else if (gate.first == "Z") {
      insert_phase(make_pair("pi", 0), wires[qbits[0]], phase_expts);
    // Wow an X gate
    } else if (gate.first == "X") {
      wires[qbits[0]].flip(n + h);
    } else if (gate.first == "Y") {
      wires[qbits[0]].flip(n + h);
      insert_phase(make_pair("pi", 0), wires[qbits[0]], phase_expts);
    // cnot name tof for some reason
    } else if (gate.first == "tof") {
      wires[qbits[1]] ^= wires[qbits[0]];
    } else if (gate.first == "Z3") {
      insert_phase(make_pair("pi", 2), wires[qbits[0]], phase_expts);
      insert_phase(make_pair("pi", 2), wires[qbits[1]], phase_expts);
      insert_phase(make_pair("pi", 2), wires[qbits[2]], phase_expts);
      insert_phase(make_pair("-pi", 2), wires[qbits[0]] ^ wires[qbits[1]], phase_expts);
      insert_phase(make_pair("-pi", 2), wires[qbits[0]] ^ wires[qbits[2]], phase_expts);
      insert_phase(make_pair("-pi", 2), wires[qbits[1]] ^ wires[qbits[2]], phase_expts);
      insert_phase(make_pair("pi", 2), wires[qbits[0]] ^ wires[qbits[1]] ^ wires[qbits[2]], phase_expts);
    // the interesting case
    } else if (gate.first == "H") {
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
    } else {
      cout << "ERROR: not a valid circuit\n";
      delete[] outputs;
    }
  }
}

//---------------------------- Synthesis

// So what we're going to do is have parallel partitions for
//  each syntactic class of rotations
dotqc character::synthesize() {
  map<string, partitioning> floats, frozen;
  dotqc ret;
  xor_func mask(n + h + 1, 0);      // Tells us what values we have prepared
  xor_func wires[n + m];        // Current state of the wires
  map<string, list<int> > remaining;          // Which terms we still have to partition
  int dim = n, tmp, j;
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


  // initialize the remaining lists and partitions
  for (map<string, pair<int, vector<exponent> > >::iterator it = phase_expts.begin();
       it != phase_expts.end(); it++) {
    for (int i = 0; i < it->second.second.size(); i++) {
      if (it->second.second[i].first != 0) {
        xor_func tmp = (~mask) & (it->second.second[i].second);
        if (tmp.none()) {
          add_to_partition(floats[it->first], i, it->second.second, oracle);
        } else {
          remaining[it->first].push_back(i);
        }
      }
    }
  }

  for (it = hadamards.begin(); it != hadamards.end(); it++) {
    // 1. freeze partitions that are not disjoint from the hadamard input
    // 2. construct CNOT+T circuit
    // 3. apply the hadamard gate
    // 4. add new functions to the partition

    for (map<string, pair<int, vector<exponent> > >::iterator ti = phase_expts.begin();
	 ti != phase_expts.end(); ti++) {
      // determine frozen partitions
      frozen[ti->first] = freeze_partitions(floats[ti->first], it->in[ti->first]);

      // Construct {CNOT, T} subcircuit for the frozen partitions
      ret.circ.splice(ret.circ.end(), 
		      construct_circuit(phase_expts[ti->first].second,
					frozen[ti->first], wires, wires, n + m, n + h, names,
					ti->first, phase_expts[ti->first].first));
    }
    ret.circ.splice(ret.circ.end(), 
		    construct_circuit(vector<exponent>(), partitioning(), wires, it->wires, 
				      n + m, n + h, names, "", 0));
    for (int i = 0; i < n + m; i++) {
      wires[i] = it->wires[i];
    }

    // Apply Hadamard gate
    ret.circ.push_back(make_pair("H", list<string>(1, names[it->qubit])));
    wires[it->qubit].reset();
    wires[it->qubit].set(it->prep);
    mask.set(it->prep);

    // Check for increases in dimension
    tmp = compute_rank(n + m, n + h, wires);
    if (tmp > dim) {
      dim = tmp;
      oracle.set_dim(dim);
      for (map<string, pair<int, vector<exponent> > >::iterator ti = phase_expts.begin();
	   ti != phase_expts.end(); ti++) {
	repartition(floats[ti->first], phase_expts[ti->first].second, oracle);
      }
    }

    // Add new functions to the partition
    for (map<string, pair<int, vector<exponent> > >::iterator it = phase_expts.begin();
	 it != phase_expts.end(); it++) {
      for (list<int>::iterator ti = remaining[it->first].begin(); ti != remaining[it->first].end();) {
        xor_func tmp = (~mask) & (it->second.second[*ti].second);
        if (tmp.none()) {
          add_to_partition(floats[it->first], *ti, it->second.second, oracle);
          ti = remaining[it->first].erase(ti);
        } else ti++;
      }
    }
  }

  // Construct the final {CNOT, T} subcircuit
  for (map<string, pair<int, vector<exponent> > >::iterator it = phase_expts.begin();
       it != phase_expts.end(); it++) {
    ret.circ.splice(ret.circ.end(), 
		    construct_circuit(phase_expts[it->first].second,
				      floats[it->first], wires, wires, n + m, n + h, names,
		   		      it->first, phase_expts[it->first].first));
  }
  ret.circ.splice(ret.circ.end(), 
		  construct_circuit(vector<exponent>(), partitioning(), wires, outputs, 
				    n + m, n + h, names, "", 0));

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
