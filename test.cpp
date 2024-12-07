#include <cstdio>
using namespace std;
const int mod = 998244353;
int n, m;
struct node {
    int x, y;
} l[(int)5e5 + 5];
int main() {
    scanf("%d%d", &n, &m);
    for (int i = 1; i <= m; i++)
        scanf("%d%d", &l[i].x, &l[i].y);

    return 0;
}