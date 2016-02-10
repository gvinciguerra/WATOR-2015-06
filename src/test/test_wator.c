#include "wator.h"
#include "unity.h"

void setUp(void)
{
  // Eseguito prima di OGNI FUNZIONE di test
}

void tearDown(void)
{
    // Eseguito dopo OGNI FUNZIONE di test
}

void test_cell_to_char()
{
    TEST_ASSERT_EQUAL('S', cell_to_char(SHARK));
    TEST_ASSERT_EQUAL('F', cell_to_char(FISH));
    TEST_ASSERT_EQUAL('W', cell_to_char(WATER));
    TEST_ASSERT_EQUAL('?', cell_to_char(42));
}

void test_char_to_cell()
{
    TEST_ASSERT_EQUAL(SHARK, char_to_cell('S'));
    TEST_ASSERT_EQUAL(FISH, char_to_cell('F'));
    TEST_ASSERT_EQUAL(WATER, char_to_cell('W'));
    TEST_ASSERT_EQUAL(-1, char_to_cell('g'));
}

void test_new_planet()
{
    TEST_ASSERT_NULL(new_planet(0,4)); // Argomento errato
    planet_t *planet = new_planet(8, 16);
    TEST_ASSERT_NOT_NULL(planet);
    TEST_ASSERT_EQUAL(WATER, planet->w[5][5]);
}

void test_print_planet()
{
    const char *tempFileName = "print_planet_test_output.txt";
    planet_t *planet = new_planet(32, 64);
    TEST_ASSERT_NOT_NULL(planet);

    // Test file corretto
    FILE *f2 = fopen(tempFileName, "w");
    TEST_ASSERT_NOT_NULL(f2);
    TEST_ASSERT_EQUAL(0, print_planet(f2, planet));

    // Test di stampa delle dimensioni della matrice
    int rows, cols;
    f2 = freopen(tempFileName, "r", f2);
    fscanf(f2, "%d\n%d", &rows, &cols);
    TEST_ASSERT_EQUAL(32, rows);
    TEST_ASSERT_EQUAL(64, cols);
    fclose(f2);
    remove(tempFileName);
}

void test_load_planet()
{
    FILE *f = fopen("test_data/esempio0.txt", "r");
    TEST_ASSERT_NOT_NULL(load_planet(f));
    fclose(f);
}

void test_shark_rule1()
{
    int destX, destY;
    wator_t *wator = new_wator("test_data/esempio0.txt");
    TEST_ASSERT_NOT_NULL(wator);

    // Test con un pesce in (2, 3)
    TEST_ASSERT_EQUAL(EAT, shark_rule1(wator, 3, 3, &destX, &destY));
    TEST_ASSERT_EQUAL(2, destX);
    TEST_ASSERT_EQUAL(3, destY);

    // Test ai limiti del pianeta circondato da squali
    TEST_ASSERT_EQUAL(STOP, shark_rule1(wator, 0, 0, &destX, &destY));
    TEST_ASSERT_EQUAL(0, destX);
    TEST_ASSERT_EQUAL(0, destY);

    // Test movimento
    TEST_ASSERT_EQUAL(MOVE, shark_rule1(wator, 5, 0, &destX, &destY));
}

void test_shark_rule2()
{
    int destX, destY;
    wator_t *wator = new_wator("test_data/esempio0.txt");
    TEST_ASSERT_NOT_NULL(wator);
    wator->sb = 0;

    // Test nascita
    TEST_ASSERT_EQUAL(9, shark_count(wator->plan));
    TEST_ASSERT_EQUAL(ALIVE, shark_rule2(wator, 3, 3, &destX, &destY));
    TEST_ASSERT_EQUAL(10, shark_count(wator->plan));

    // Test morte
    wator->plan->dtime[3][3] = 1000;
    TEST_ASSERT_EQUAL(DEAD, shark_rule2(wator, 3, 3, &destX, &destY));
    TEST_ASSERT_EQUAL(WATER, wator->plan->w[3][3]);
}

void test_fish_rule3()
{
    int destX, destY;
    wator_t *wator = new_wator("test_data/esempio0.txt");
    TEST_ASSERT_NOT_NULL(wator);

     // Test circondato da squali e pesci
    TEST_ASSERT_EQUAL(STOP, fish_rule3(wator, 6, 12, &destX, &destY));
    TEST_ASSERT_EQUAL(6, destX);
    TEST_ASSERT_EQUAL(12, destY);

    // Test unica via libera a sinistra
    TEST_ASSERT_EQUAL(MOVE, fish_rule3(wator, 5, 12, &destX, &destY));
    TEST_ASSERT_EQUAL(5, destX);
    TEST_ASSERT_EQUAL(11, destY);
}

void test_fish_rule4()
{
    int destX, destY;
    wator_t *wator = new_wator("test_data/esempio0.txt");
    TEST_ASSERT_NOT_NULL(wator);

    // Test a vuoto, senza nascita
    TEST_ASSERT_EQUAL(0, fish_rule4(wator, 2, 3, &destX, &destY));

    // Test nascita
    wator->plan->btime[2][3] = 1000;
    TEST_ASSERT_EQUAL(6, fish_count(wator->plan));
    TEST_ASSERT_EQUAL(0, fish_rule4(wator, 2, 3, &destX, &destY));
    TEST_ASSERT_EQUAL(7, fish_count(wator->plan));
}

void test_move_cell()
{
    FILE *f = fopen("test_data/esempio0.txt", "r");
    planet_t *p = load_planet(f);
    TEST_ASSERT_NOT_NULL(p);

    // Test spostamento squalo
    move_cell(p, 5, 0, 4, 0);
    TEST_ASSERT_EQUAL(WATER, p->w[5][0]);
    TEST_ASSERT_EQUAL(SHARK, p->w[4][0]);

    // Test spostamento pesce
    move_cell(p, 5, 12, 5, 11);
    TEST_ASSERT_EQUAL(WATER, p->w[5][12]);
    TEST_ASSERT_EQUAL(FISH, p->w[5][11]);
    fclose(f);
}
