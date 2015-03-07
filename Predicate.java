import java.util.ArrayList;

//Represents a single predicate and its arguments
public class Predicate {
	
	private String name;
	private ArrayList<String> args;
	private int numArgs;
	
	public Predicate(String name, ArrayList<String> args) {
		this.name = name;
		this.args = args;
		this.numArgs = args.size();
	}
	
	public String getName() {
		return this.name;
	}
	
	public ArrayList<String> getArgs() {
		ArrayList<String> copy = new ArrayList<String>();
		for(String arg : this.args) {
			copy.add(arg);
		}
		return copy;
	}
	
	public int getNumArgs() {
		return this.numArgs;
	}
	
	public boolean hasVariables() {
		boolean hasVars = false;
		for(String currVar : this.args) {
			if(Character.isLowerCase((currVar.charAt(0)))) {
				hasVars = true;
			}
		}
		return hasVars;
	}
	
	public int getVarIndex() {
		int varIndex = -1;
		for(String currVar : this.args) {
			if(Character.isLowerCase((currVar.charAt(0)))) {
				varIndex = args.indexOf(currVar);
			}
		}
		return varIndex;
	}
	
	public boolean equals(Predicate other) {
		boolean equal = true;
		
		//Check that the number of args is equal
		int thisArgSize = this.args.size();
		int otherArgSize = other.getArgs().size();
		if(thisArgSize != otherArgSize) {
			equal = false;
		}
		else {//Check args are the same
			for(int i = 0; i < thisArgSize; i++) {
				if(!this.args.get(i).equals(other.getArgs().get(i))) {
					equal = false;
				}
			}
		}
		
		return equal;
	}
}
