import java.util.ArrayList;

public class Substitution {
	private String variable;
	private String constant;
	
	public Substitution(Predicate rhs, Predicate goal) { 
		//We know by this point that name of RHS = name of goal, and that one or both have a variable
		int varIndex = rhs.getVarIndex();
		if(varIndex == -1) {//sanity check
			varIndex = goal.getVarIndex();
		}
		
		this.variable = rhs.getArgs().get(varIndex);
		this.constant = goal.getArgs().get(varIndex);
	}
	
	public String getVariable() {
		return this.variable;
	}
	
	public String getConstant() {
		return this.constant;
	}
	
	public boolean equals(Substitution otherSub) {
		boolean equal = true;
		if(!this.variable.equals(otherSub.getVariable())) {
			equal = false;
		}
		if(!this.constant.equals(otherSub.getConstant())) {
			equal = false;
		}
		
		return equal;
	}
	
	public Predicate makeSubstitution(Predicate pred) {
		Predicate subbed = null;
		
		if(pred.hasVariables()) {
			int index = pred.getVarIndex();
			String var = pred.getArgs().get(index);
			
			if(var.equals(this.variable)) { //make sure variable in substitution = variable in predicate
				ArrayList<String> newArgs = pred.getArgs();
				newArgs.set(index, this.constant);
				subbed = new Predicate(pred.getName(), newArgs);
			}
		}
		
		return subbed;
	}
}
