#create = "CREATE TABLE Map (roomID varchar(255), x int, y int, z int, trans int);"
#print create

maxX = 10
maxY = 10
maxZ = 3

insert = ""
for z in range(0,maxZ):
	for y in range(0,maxY):
		for x in range(0,maxX):
			trans = True #room transparent?
			if((x == 1 or x == 8) and (y == 1 or y == 8)):
				trans = False
			insert = "INSERT INTO Map (roomID, trans) VALUES (" + \
			"'" + str(x) + str(y) + str(z) + "'" + ", " + "'" + str(trans) + "');"
			print insert