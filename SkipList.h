//
// Created by 27262 on 2024/5/29.
//

#ifndef THREADPOOL_SKIPLIST_H
#define THREADPOOL_SKIPLIST_H

#include <vector>
#include <cstdlib>

using namespace std;

class Skiplist {
public:
    static const int level = 8; // 层数，经验值 8，太大浪费空间，因为每一个节点都要存在每一层的 next，层数越多节点数越多

    // 定义跳表节点
    struct Node {
        int val; // 节点值
        vector<Node *> next; // 记录节点在每一层的 next，next[i] 表示当前节点第 i 层的 next

        Node(int _val) : val(_val) { // 构造函数
            next.resize(level, nullptr); // 初始化 next 数组的大小和层数 level 相同，初始值都指向 NULL
        }
    } *head; // 定义头节点 head

    Skiplist() {
        head = new Node(-1); // 初始化一个不存在的节点值 -1
    }

    ~Skiplist() {
        delete head; // 析构函数删除 head
    }

    // 辅助函数：找到每一层 i 小于目标值 target 的最大节点 pre[i]，最后 pre 中存的就是每一层小于 target 的最大节点
    void find(int target, vector<Node *> &pre) {
        auto p = head; // 从头节点开始遍历每一层
        for (int i = level - 1; i >= 0; i--) { // 从上层往下层找
            while (p->next[i] && p->next[i]->val < target)
                p = p->next[i]; // 如果当前层 i 的 next 不为空，且它的值小于 target，则 p 往后走指向这一层 p 的 next
            pre[i] = p; // 退出 while 时说明找到了第 i 层小于 target 的最大节点就是 p
        }
    }

    // 从跳表中查找 target
    bool search(int target) {
        vector<Node *> pre(level);
        find(target, pre); // 先找到每一层 i 小于目标值 target 的最大节点 pre[i]

        auto p = pre[0]->next[0]; // 因为最下层【0】的节点是全的，所以只需要判断 target 是否在第 0 层即可，而 pre[0] 正好就是小于 target 的最大节点，如果 pre[0]->next[0] 的值不是 target 说明没有这个元素
        return p && p->val == target;
    }

    // 向跳表中插入元素 num
    void add(int num) {
        vector<Node *> pre(level);
        find(num, pre); // 先找到每一层 i 小于目标值 target 的最大节点 pre[i]

        auto p = new Node(num); // 创建要插入的新节点
        for (int i = 0; i < level; i++) { // 遍历每一层，从下往上插入新节点
            p->next[i] = pre[i]->next[i]; // 这两步就是单链表的插入
            pre[i]->next[i] = p;
            if (rand() % 2) break; // 每一层有 50% 的概率不插入新节点
        }
    }

    // 从跳表中删除 num
    bool erase(int num) {
        vector<Node *> pre(level);
        find(num, pre); // 先找到每一层 i 小于目标值 target 的最大节点 pre[i]

        // 先判断 num 是否存在，不存在直接返回 false
        // 第 0 层存储的是全部节点，所以只需要判断 pre[0]->next[0]（第 0 层小于 num 的最大节点的在第 0 层的 next） 是不是 num 即可
        auto p = pre[0]->next[0];
        if (!p || p->val != num) return false;

        // 否则删除每一层的 num，如果 pre[i]->next[i] == p 说明第 i 层存在 p
        for (int i = 0; i < level && pre[i]->next[i] == p; i++) {
            pre[i]->next[i] = p->next[i]; // 单链表删除
        }

        delete p; // 删除节点 p，防止内存泄漏

        return true;
    }
};

/**
 * Your Skiplist object will be instantiated and called as such:
 * Skiplist* obj = new Skiplist();
 * bool param_1 = obj->search(target);
 * obj->add(num);
 * bool param_3 = obj->erase(num);
 */

#endif //THREADPOOL_SKIPLIST_H
