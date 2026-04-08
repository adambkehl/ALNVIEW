/* Tests for hash.c: hash table operations, string pool management */

#include "test_harness.h"
#include "hash.h"

TEST(hash_create_and_free)
{
    Hash_Table *ht = New_Hash_Table(16, 1);
    ASSERT_NOT_NULL(ht);
    Free_Hash_Table(ht);
}

TEST(hash_add_and_lookup)
{
    Hash_Table *ht = New_Hash_Table(16, 1);
    ASSERT_NOT_NULL(ht);

    int idx = Hash_Add(ht, "scaffold_1");
    ASSERT_TRUE(idx >= 0);

    int found = Hash_Lookup(ht, "scaffold_1");
    ASSERT_EQ(found, idx);

    int not_found = Hash_Lookup(ht, "nonexistent");
    ASSERT_EQ(not_found, -1);

    Free_Hash_Table(ht);
}

TEST(hash_duplicate_add)
{
    Hash_Table *ht = New_Hash_Table(16, 1);
    int idx1 = Hash_Add(ht, "key");
    int idx2 = Hash_Add(ht, "key");
    /* Adding the same key again should return same index */
    ASSERT_EQ(idx1, idx2);
    Free_Hash_Table(ht);
}

TEST(hash_empty_string)
{
    Hash_Table *ht = New_Hash_Table(16, 1);
    int idx = Hash_Add(ht, "");
    ASSERT_TRUE(idx >= 0);
    char *s = Get_Hash_String(ht, idx);
    ASSERT_STR_EQ(s, "");
    Free_Hash_Table(ht);
}

TEST(hash_many_entries_trigger_realloc)
{
    /* Insert enough entries to force internal reallocation of cells/strings */
    Hash_Table *ht = New_Hash_Table(4, 1);  /* start very small */
    ASSERT_NOT_NULL(ht);

    char buf[64];
    for (int i = 0; i < 200; i++) {
        snprintf(buf, sizeof(buf), "scaffold_%d_with_long_name_padding", i);
        int idx = Hash_Add(ht, buf);
        ASSERT_TRUE(idx >= 0);
    }

    /* Verify all entries still accessible after reallocs */
    for (int i = 0; i < 200; i++) {
        snprintf(buf, sizeof(buf), "scaffold_%d_with_long_name_padding", i);
        int found = Hash_Lookup(ht, buf);
        ASSERT_TRUE(found >= 0);
        char *s = Get_Hash_String(ht, found);
        ASSERT_STR_EQ(s, buf);
    }

    ASSERT_EQ(Get_Hash_Size(ht), 200);
    Free_Hash_Table(ht);
}

TEST(hash_very_long_key)
{
    Hash_Table *ht = New_Hash_Table(16, 1);
    /* Key much longer than initial string pool */
    char longkey[4096];
    memset(longkey, 'X', sizeof(longkey) - 1);
    longkey[sizeof(longkey) - 1] = '\0';

    int idx = Hash_Add(ht, longkey);
    ASSERT_TRUE(idx >= 0);

    char *s = Get_Hash_String(ht, idx);
    ASSERT_STR_EQ(s, longkey);

    Free_Hash_Table(ht);
}

TEST(hash_no_keep_mode)
{
    /* With keep=0, hash stores pointer, not copy */
    Hash_Table *ht = New_Hash_Table(16, 0);
    ASSERT_NOT_NULL(ht);

    char key[] = "mutable_key";
    int idx = Hash_Add(ht, key);
    ASSERT_TRUE(idx >= 0);

    int found = Hash_Lookup(ht, "mutable_key");
    ASSERT_EQ(found, idx);

    Free_Hash_Table(ht);
}

TEST(hash_clear)
{
    Hash_Table *ht = New_Hash_Table(16, 1);
    Hash_Add(ht, "a");
    Hash_Add(ht, "b");
    ASSERT_EQ(Get_Hash_Size(ht), 2);

    Clear_Hash_Table(ht);
    ASSERT_EQ(Get_Hash_Size(ht), 0);

    /* After clear, old keys should not be found */
    ASSERT_EQ(Hash_Lookup(ht, "a"), -1);
    ASSERT_EQ(Hash_Lookup(ht, "b"), -1);

    /* Should be able to add keys again */
    int idx = Hash_Add(ht, "c");
    ASSERT_TRUE(idx >= 0);

    Free_Hash_Table(ht);
}

TEST_MAIN()
