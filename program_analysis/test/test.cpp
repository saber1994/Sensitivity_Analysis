#include <set>
#include <algorithm>
#include <iostream>
using namespace std;
int main(){
	set<int> a;
	set<int> b;
	int x[3] = {1,2,3};
	int y[7] = {3,4,5,6,7,8,9};
	a.insert(x,x+3);
	b.insert(y,y+7);
	set_union(a.begin(), a.end(), b.begin(), b.end(), std::inserter(a,a.begin()));
	cout<<a.size()<<endl;
	for(int value : a)
	{	
		cout<<value;
	}
 	return 0;
}
